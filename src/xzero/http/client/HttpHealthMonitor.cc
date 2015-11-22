#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/io/InputStream.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero/JsonWriter.h>
#include <algorithm>

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.client.HttpHealthMonitor", msg)
# define TRACE(msg...) logTrace("http.client.HttpHealthMonitor", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

namespace xzero {

template<>
std::string StringUtil::toString(http::client::HttpHealthMonitor::State state) {
  switch (state) {
    case http::client::HttpHealthMonitor::State::Undefined:
      return "undefined";
    case http::client::HttpHealthMonitor::State::Offline:
      return "offline";
    case http::client::HttpHealthMonitor::State::Online:
      return "online";
  }
}

namespace http {
namespace client {

HttpHealthMonitor::HttpHealthMonitor(Executor* executor,
                                     const IPAddress& ipaddr,
                                     int port,
                                     const Uri& testUrl,
                                     Duration interval,
                                     unsigned successThreshold,
                                     const std::vector<HttpStatus>& successCodes,
                                     Duration connectTimeout,
                                     Duration readTimeout,
                                     Duration writeTimeout,
                                     StateChangeNotify onStateChange)
    : executor_(executor),
      timerHandle_(),
      ipaddr_(ipaddr),
      port_(port),
      testUrl_(testUrl),
      interval_(interval),
      successCodes_(successCodes),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      successThreshold_(successThreshold),
      onStateChange_(onStateChange),
      state_(State::Undefined),
      totalFailCount_(0),
      consecutiveSuccessCount_(0),
      totalOfflineTime_(Duration::Zero),
      client_() {
  TRACE("ctor: $0:$1", ipaddr_, port_);
  start();
}

HttpHealthMonitor::~HttpHealthMonitor() {
  stop();
}

void HttpHealthMonitor::setStateChangeCallback(StateChangeNotify notify) {
  onStateChange_ = notify;
}

void HttpHealthMonitor::start() {
  onCheckNow();
}

void HttpHealthMonitor::stop() {
  TRACE("stop");
  if (timerHandle_) {
    timerHandle_->cancel();
  }
}

void HttpHealthMonitor::recheck() {
  TRACE("recheck");
  timerHandle_ = executor_->executeAfter(
      interval_,
      std::bind(&HttpHealthMonitor::onCheckNow, this));
}

void HttpHealthMonitor::logSuccess() {
  DEBUG("logSuccess!");
  ++consecutiveSuccessCount_;

  if (consecutiveSuccessCount_ >= successThreshold_) {
    TRACE("The successThreshold reached. Going online.");
    setState(State::Online);
  }

  recheck();
}

void HttpHealthMonitor::logFailure() {
  ++totalFailCount_;
  consecutiveSuccessCount_ = 0;
  DEBUG("logFailure $0", totalFailCount_);

  setState(State::Offline);

  recheck();
}

/**
 * Forces a health-state change.
 */
void HttpHealthMonitor::setState(State value) {
  assert(value != State::Undefined && "Setting state to Undefined is not allowed.");
  if (state_ == value)
    return;

  DEBUG("setState $0 -> $1", state_, value);

  State oldState = state_;
  state_ = value;

  if (onStateChange_) {
    onStateChange_(this, oldState);
  }

  // if (state_ == State::Offline) {
  //   executor_->execute(std::bind(&HttpHealthMonitor::start, this));
  // }
}

void HttpHealthMonitor::onCheckNow() {
  DEBUG("onCheckNow");

  timerHandle_.reset();

  Future<RefPtr<InetEndPoint>> ep =
      InetEndPoint::connectAsync(ipaddr_, port_, connectTimeout(), executor_);

  ep.onFailure(std::bind(&HttpHealthMonitor::onConnectFailure, this, std::placeholders::_1));
  ep.onSuccess(std::bind(&HttpHealthMonitor::onConnected, this, std::placeholders::_1));
}

void HttpHealthMonitor::onConnectFailure(Status status) {
  DEBUG("Connecting to backend failed. $0", status);
  logFailure();
}

void HttpHealthMonitor::onConnected(const RefPtr<InetEndPoint>& ep) {
  DEBUG("onConnected");
  client_ = std::unique_ptr<HttpClient>(new HttpClient(executor_, ep.as<EndPoint>()));

  BufferRef requestBody;

  HttpRequestInfo requestInfo(HttpVersion::VERSION_1_1,
                              HttpMethod::GET,
                              testUrl_.pathAndQuery(),
                              requestBody.size(),
                              { {"Host", testUrl_.hostAndPort()},
                                {"User-Agent", "HttpHealthMonitor"} } );

  client_->send(std::move(requestInfo), requestBody);
  Future<HttpClient*> f = client_->completed();
  f.onFailure(std::bind(&HttpHealthMonitor::onRequestFailure, this, std::placeholders::_1));
  f.onSuccess(std::bind(&HttpHealthMonitor::onResponseReceived, this, std::placeholders::_1));
}

void HttpHealthMonitor::onRequestFailure(Status status) {
  DEBUG("onRequestFailure");
  logFailure();
}

void HttpHealthMonitor::onResponseReceived(HttpClient* client) {
  DEBUG("onResponseReceived");
  auto i = std::find(successCodes_.begin(),
                     successCodes_.end(),
                     client->responseInfo().status());
  if (i == successCodes_.end()) {
    DEBUG("received bad response status code. $0 $1",
          (int) client->responseInfo().status(),
          client->responseInfo().status());
    logFailure();
    return;
  }

  logSuccess();
}

JsonWriter& operator<<(JsonWriter& json, const HttpHealthMonitor& monitor) {
  json.beginObject()
      .name("state")(StringUtil::toString(monitor.state()))
      .name("interval")(monitor.interval().milliseconds())
      .endObject();

  return json;
}

} // namespace client
} // namespace http
} // namespace xzero
