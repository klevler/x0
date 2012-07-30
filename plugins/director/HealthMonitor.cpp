#include "HealthMonitor.h"
#include <cassert>
#include <cstdarg>

/*
 * TODO
 * - connect/read/write timeout handling.
 * - proper error reporting.
 * - properly support all modes
 *   - Opportunistic
 *   - Lazy
 */

using namespace x0;

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->debug(msg))
#else
#	define TRACE(msg...) do { } while (0)
#endif

HealthMonitor::HealthMonitor(HttpWorker* worker) :
	Logging("HealthMonitor"),
	HttpMessageProcessor(HttpMessageProcessor::RESPONSE),
	mode_(Mode::Paranoid),
	worker_(worker),
	socketSpec_(),
	socket_(worker_->loop()),
	interval_(TimeSpan::fromSeconds(2)),
	state_(State::Undefined),
	onStateChange_(),
	request_(),
	writeOffset_(0),
	response_(),
	responseCode_(0),
	processingDone_(false),
	expectCode_(200),
	timer_(worker_->loop()),
	successThreshold(2),
	failCount_(0),
	successCount_(0)
{
	timer_.set<HealthMonitor, &HealthMonitor::onCheckStart>(this);

	// initialize request message with some reasonable default.
	setRequest(
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Health-Check: yes\r\n"
		"\r\n"
	);
}

HealthMonitor::~HealthMonitor()
{
	stop();
}

const std::string& HealthMonitor::mode_str() const
{
	static const std::string modeStr[] = {
		"paranoid", "opportunistic", "lazy"
	};

	return modeStr[static_cast<size_t>(mode_)];
}

/**
 * Sets monitoring mode.
 */
void HealthMonitor::setMode(Mode value)
{
	if (mode_ == value)
		return;

	mode_ = value;
}

const std::string& HealthMonitor::state_str() const
{
	static const std::string stateStr[] = {
		"undefined", "offline", "online"
	};

	return stateStr[static_cast<size_t>(state_)];
}

/**
 * Forces a health-state change.
 */
void HealthMonitor::setState(State value)
{
	assert(value != State::Undefined && "Setting state to Undefined is not allowed.");
	if (state_ == value)
		return;

	state_ = value;

	TRACE("setState: %s", state_str().c_str());

	if (onStateChange_) {
		onStateChange_(this);
	}

	if (state_ == State::Offline) {
		worker_->post<HealthMonitor, &HealthMonitor::start>(this);
	}
}

/**
 * Sets the callback to be invoked on health state changes.
 */
void HealthMonitor::onStateChange(const std::function<void(HealthMonitor*)>& callback)
{
	onStateChange_ = callback;
}

void HealthMonitor::setTarget(const SocketSpec& value)
{
	socketSpec_ = value;

#ifndef NDEBUG
	setLoggingPrefix("HealthMonitor/%s", socketSpec_.str().c_str());
#endif
}

void HealthMonitor::setInterval(const TimeSpan& value)
{
	interval_ = value;
}

/** Sets the raw HTTP request, used to perform the health check.
 */
void HealthMonitor::setRequest(const char* fmt, ...)
{
	va_list va;
	size_t blen = std::min(request_.capacity(), 1023lu);

	do {
		request_.reserve(blen + 1);
		va_start(va, fmt);
		blen = vsnprintf(const_cast<char*>(request_.data()), request_.capacity(), fmt, va);
		va_end(va);
	} while (blen >= request_.capacity());

	request_.resize(blen);
}

/**
 * Starts health-monitoring an HTTP server.
 */
void HealthMonitor::start()
{
	TRACE("start()");

	socket_.close();

	writeOffset_ = 0;
	response_.clear();
	responseCode_ = 0;
	processingDone_ = false;

	timer_.start(interval_.value(), 0.0);
}

/**
 * Stops any active timer or health-check operation.
 */
void HealthMonitor::stop()
{
	TRACE("stop()");

	timer_.stop();
	socket_.close();
}

/**
 * Callback, timely invoked when a health check is to be started.
 */
void HealthMonitor::onCheckStart()
{
	TRACE("onCheckStart()");

	socket_.open(socketSpec_, O_NONBLOCK | O_CLOEXEC);

	if (!socket_.isOpen()) {
		TRACE("Connect failed. %s", strerror(errno));
		logFailure();
	} else if (socket_.state() == Socket::Connecting) {
		TRACE("connecting asynchronously.");
		socket_.setReadyCallback<HealthMonitor, &HealthMonitor::onConnectDone>(this);
	} else {
		socket_.setReadyCallback<HealthMonitor, &HealthMonitor::io>(this);
		TRACE("connected.");
	}
}

/**
 * Callback, invoked on completed asynchronous connect-operation.
 */
void HealthMonitor::onConnectDone(Socket*, int revents)
{
	TRACE("onConnectDone(0x%04x)", revents);

	if (socket_.state() == Socket::Operational) {
		TRACE("connected");
		socket_.setReadyCallback<HealthMonitor, &HealthMonitor::io>(this);
		socket_.setMode(Socket::ReadWrite);
	} else {
		TRACE("Asynchronous connect failed %s", strerror(errno));
		logFailure();

		recheck();
	}
}

/**
 * Callback, invoked on I/O readiness of origin server connection.
 */
void HealthMonitor::io(Socket*, int revents)
{
	TRACE("io(0x%04x)", revents);

	if (revents & ev::WRITE) {
		writeSome();
	}

	if (revents & ev::READ) {
		readSome();
	}
}

/**
 * Writes the request chunk to the origin server.
 */
void HealthMonitor::writeSome()
{
	TRACE("writeSome()");

	size_t chunkSize = request_.size() - writeOffset_;
	ssize_t writeCount = socket_.write(request_.data() + writeOffset_, chunkSize);

	if (writeCount < 0) {
		TRACE("write failed. %s", strerror(errno));
		logFailure();

		recheck();
	} else {
		writeOffset_ += writeCount;

		if (writeOffset_ == request_.size()) {
			socket_.setMode(Socket::Read);
		}
	}
}

/**
 * Reads and processes a response chunk from origin server.
 */
void HealthMonitor::readSome()
{
	TRACE("readSome()");

	size_t lower_bound = response_.size();
	if (lower_bound == response_.capacity())
		response_.setCapacity(lower_bound + 4096);

	ssize_t rv = socket_.read(response_);

	if (rv > 0) {
		TRACE("readSome: read %zi bytes", rv);
		size_t np = process(response_.ref(lower_bound, rv));

		(void) np;
		TRACE("readSome(): processed %ld of %ld bytes", np, rv);

		if (HttpMessageProcessor::state() == HttpMessageProcessor::SYNTAX_ERROR) {
			TRACE("syntax error");
			logFailure();
			recheck();
		} else if (processingDone_) {
			TRACE("processing done");
			recheck();
		} else {
			TRACE("resume with io:%d, state:%s", socket_.mode(), state_str().c_str());
			socket_.setMode(Socket::Read);
		}
	} else if (rv == 0) {
		TRACE("remote endpoint closed connection.");
	} else {
		switch (errno) {
			case EAGAIN:
			case EINTR:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
			case EWOULDBLOCK:
#endif
				break;
			default:
				TRACE("error reading health-check response from backend. %s", strerror(errno));
				recheck();
				return;
		}
	}
}

/**
 * Origin server timed out in read or write operation.
 */
void HealthMonitor::onTimeout()
{
	TRACE("onTimeout()");

	// TODO
}

void HealthMonitor::recheck()
{
	TRACE("recheck()");
	start();
}

/**
 * Callback, invoked on successfully parsed response status line.
 */
bool HealthMonitor::onMessageBegin(int versionMajor, int versionMinor, int code, const BufferRef& text)
{
	TRACE("onMessageBegin: (HTTP/%d.%d, %d, '%s')", versionMajor, versionMinor, code, text.str().c_str());

	responseCode_ = code;

	return true;
}

/**
 * Callback, invoked on each successfully parsed response header key/value pair.
 */
bool HealthMonitor::onMessageHeader(const BufferRef& name, const BufferRef& value)
{
	// do nothing with response message headers
	return true;
}

/**
 * Callback, invoked on each partially or fully parsed response body chunk.
 */
bool HealthMonitor::onMessageContent(const BufferRef& chunk)
{
	// do nothing with response body chunk
	return true;
}

/**
 * Callback, invoked when the response message has been fully parsed.
 */
bool HealthMonitor::onMessageEnd()
{
	TRACE("onMessageEnd() state:%s", state_str().c_str());
	processingDone_ = true;

	if (responseCode_ == expectCode_) {
		logSuccess();
	} else {
		logFailure();
	}

	// stop processing
	return false;
}

void HealthMonitor::logSuccess()
{
	++successCount_;

	if (successCount_ >= successThreshold) {
		TRACE("onMessageEnd: successThreshold reached.");
		setState(State::Online);
	}
}

void HealthMonitor::logFailure()
{
	++failCount_;
	successCount_ = 0;

	setState(State::Offline);
}

Buffer& operator<<(Buffer& output, const HealthMonitor& monitor)
{
	output
		<< "{"
		<< "\"mode\": \"" << monitor.mode_str() << "\", "
		<< "\"state\": \"" << monitor.state_str() << "\", "
		<< "\"interval\": " << monitor.interval().totalMilliseconds()
		<< "}";

	return output;
}
