#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/io/StringInputStream.h>
#include <xzero/io/InputStream.h>
#include <xzero/logging.h>
#include <algorithm>

namespace xzero {
namespace http {
namespace client {

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.client.HttpCluster", msg)
# define TRACE(msg...) logTrace("http.client.HttpCluster", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

HttpCluster::HttpCluster(const std::string& name, Executor* executor)
    : HttpCluster(name,
                  executor,
                  true,                       // enabled
                  false,                      // stickyOfflineMode
                  true,                       // allowXSendfile
                  true,                       // enqueueOnUnavailable
                  1000,                       // queueLimit
                  Duration::fromSeconds(30),  // queueTimeout
                  Duration::fromSeconds(30),  // retryAfter
                  3,                          // maxRetryCount
                  Duration::fromSeconds(4),   // backend connect timeout
                  Duration::fromSeconds(30),  // backend response read timeout
                  Duration::fromSeconds(8)) { // backend request write timeout
}

HttpCluster::HttpCluster(const std::string& name,
                         Executor* executor,
                         bool enabled,
                         bool stickyOfflineMode,
                         bool allowXSendfile,
                         bool enqueueOnUnavailable,
                         size_t queueLimit,
                         Duration queueTimeout,
                         Duration retryAfter,
                         size_t maxRetryCount,
                         Duration connectTimeout,
                         Duration readTimeout,
                         Duration writeTimeout)
    : name_(name),
      enabled_(enabled),
      stickyOfflineMode_(stickyOfflineMode),
      allowXSendfile_(allowXSendfile),
      enqueueOnUnavailable_(enqueueOnUnavailable),
      queueLimit_(queueLimit),
      queueTimeout_(queueTimeout),
      retryAfter_(retryAfter),
      maxRetryCount_(maxRetryCount),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      executor_(executor),
      storagePath_("TODO"), // TODO
      shaper_(executor, 0),
      members_(),
      scheduler_()
{
  shaper_.setTimeoutHandler(
      std::bind(&HttpCluster::onTimeout, this, std::placeholders::_1));
}

HttpCluster::~HttpCluster() {
}

std::string HttpCluster::configuration() const {
  return ""; // TODO
}

void HttpCluster::setConfiguration(const std::string& text) {
  // TODO
}

void HttpCluster::addMember(const IPAddress& ipaddr, int port, size_t capacity) {
  addMember(StringUtil::format("$0:$1", ipaddr, port),
            ipaddr, port, capacity, true);
}

void HttpCluster::addMember(const std::string& name,
                            const IPAddress& ipaddr,
                            int port,
                            size_t capacity,
                            bool enabled) {
  Executor* executor = executor_; // TODO: get as function arg for passing:  daemon().selectClientScheduler()
  std::string protocol = "http";  // TODO: get as function arg
  int healthCheckSuccessThreshold = 3;
  Duration healthCheckInterval = Duration::fromSeconds(4);
  const std::vector<HttpStatus> healthCheckSuccessCodes = { HttpStatus::Ok };

  TRACE("addMember: $0 $1:$2", name, ipaddr, port);
  std::unique_ptr<HttpHealthMonitor> healthMonitor(
      new HttpHealthMonitor(executor, ipaddr, port, healthCheckUri(),
                    healthCheckInterval,
                    healthCheckSuccessThreshold,
                    healthCheckSuccessCodes,
                    connectTimeout(),
                    readTimeout(),
                    writeTimeout()));

  HttpClusterMember* backend = new HttpClusterMember(executor,
                                                     name,
                                                     ipaddr,
                                                     port,
                                                     capacity,
                                                     enabled,
                                                     protocol,
                                                     connectTimeout(),
                                                     readTimeout(),
                                                     writeTimeout(),
                                                     std::move(healthMonitor));

  backend->healthMonitor()->setStateChangeCallback(
      [this, backend] (HttpHealthMonitor*, HttpHealthMonitor::State oldState) {
    onBackendStateChanged(backend, backend->healthMonitor(), oldState);
  });

  members_.push_back(backend);
}

void HttpCluster::onBackendStateChanged(HttpClusterMember* backend,
                                        HttpHealthMonitor* healthMonitor,
                                        HttpHealthMonitor::State oldState) {
  TRACE("onBackendStateChanged: health=$0 -> $1, enabled=$2",
        oldState,
        backend->healthMonitor()->state(),
        backend->isEnabled());

  logInfo("HttpCluster",
          "$0: backend '$1' is now $2.",
          name(), backend->name(), healthMonitor->state());

  if (healthMonitor->isOnline()) {
    if (!backend->isEnabled())
      return;

    // backend is online and enabled

    TRACE("onBackendStateChanged: adding capacity to shaper ($0 + $1)",
           shaper()->size(), backend->capacity());
    shaper()->resize(shaper()->size() + backend->capacity());

    if (!stickyOfflineMode()) {
      // try delivering a queued request
      dequeueTo(backend);
    } else {
      // disable backend due to sticky-offline mode
      logNotice(
          "HttpCluster",
          "$0: backend '$1' disabled due to sticky offline mode.",
          name(), backend->name());
      backend->setEnabled(false);
    }
  } else if (backend->isEnabled() && oldState == HttpHealthMonitor::State::Online) {
    // backend is offline and enabled
    shaper()->resize(shaper()->size() - backend->capacity());

    TRACE("onBackendStateChanged: removing capacity from shaper ($0 - $1)",
          shaper()->size(), backend->capacity());
  }
}

HttpClusterMember* HttpCluster::findMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const HttpClusterMember* m) -> bool { return m->name() == name; });
  if (i != members_.end()) {
    return *i;
  } else {
    return nullptr;
  }
}

void HttpCluster::removeMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const HttpClusterMember* m) -> bool { return m->name() == name; });
  if (i != members_.end()) {
    delete *i;
    members_.erase(i);
  }
}

void HttpCluster::setExecutor(Executor* executor) {
  executor_ = executor;
  shaper()->setExecutor(executor);
}

TokenShaperError HttpCluster::createBucket(const std::string& name, float rate,
                                            float ceil) {
  return shaper_.createNode(name, rate, ceil);
}

HttpCluster::Bucket* HttpCluster::findBucket(const std::string& name) const {
  return shaper_.findNode(name);
}

bool HttpCluster::eachBucket(std::function<bool(Bucket*)> body) {
  for (auto& node : *shaper_.rootNode())
    if (!body(node))
      return false;

  return true;
}

void HttpCluster::schedule(HttpClusterRequest* cr) {
  schedule(cr, rootBucket());
}

void HttpCluster::schedule(HttpClusterRequest* cr, Bucket* bucket) {
  cr->bucket = bucket;

  if (!enabled_) {
    serviceUnavailable(cr);
    return;
  }

  if (cr->bucket->get(1)) {
    HttpClusterSchedulerStatus status = clusterScheduler()->schedule(cr);
    if (status == HttpClusterSchedulerStatus::Success)
      return;

    cr->bucket->put(1);
    cr->tokens = 0;

    if (status == HttpClusterSchedulerStatus::Unavailable &&
        !enqueueOnUnavailable_) {
      serviceUnavailable(cr);
    } else {
      tryEnqueue(cr);
    }
  } else if (cr->bucket->ceil() > 0 || enqueueOnUnavailable_) {
    // there are tokens available (for rent) and we prefer to wait if unavailable
    tryEnqueue(cr);
  } else {
    serviceUnavailable(cr);
  }
}

void HttpCluster::reschedule(HttpClusterRequest* cr) {
  if (verifyTryCount(cr)) {
    HttpClusterSchedulerStatus status = clusterScheduler()->schedule(cr);

    if (status != HttpClusterSchedulerStatus::Success) {
      tryEnqueue(cr);
    }
  }
}

/**
 * Verifies number of tries, this request has been attempted to be queued, to be
 * in valid range.
 *
 * @retval true tryCount is still below threashold, so further tries are allowed.
 * @retval false tryCount exceeded limit and a 503 client response has been
 *               sent. Dropped-stats have been incremented.
 */
bool HttpCluster::verifyTryCount(HttpClusterRequest* cr) {
  if (cr->tryCount <= maxRetryCount())
    return true;

  TRACE("proxy.cluster %s: request failed %d times.", name().c_str(), cr->tryCount);
  serviceUnavailable(cr);
  return false;
}

void HttpCluster::serviceUnavailable(HttpClusterRequest* cr, HttpStatus status) {
  cr->responseListener->onMessageBegin(
      HttpVersion::VERSION_1_1,
      status,
      BufferRef(StringUtil::toString(HttpStatus::ServiceUnavailable)));

  // TODO: put into a more generic place where it affects all responses.
  //
  if (cr->bucket) {
    cr->responseListener->onMessageHeader(BufferRef("Cluster-Bucket"),
                                          BufferRef(cr->bucket->name()));
  }

  if (retryAfter() != Duration::Zero) {
    char value[64];
    int vs = snprintf(value, sizeof(value), "%lu", retryAfter().seconds());
    cr->responseListener->onMessageHeader(
        BufferRef("Retry-After"), BufferRef(value, vs));
  }

  cr->responseListener->onMessageHeaderEnd();
  cr->responseListener->onMessageEnd();

  ++dropped_;
}

/**
 * Attempts to enqueue the request, respecting limits.
 *
 * Attempts to enqueue the request on the associated bucket.
 * If enqueuing fails, it instead finishes the request with a 503 (Service
 * Unavailable).
 *
 * @retval true request could be enqueued.
 * @retval false request could not be enqueued. A 503 error response has been
 *               sent out instead.
 */
bool HttpCluster::tryEnqueue(HttpClusterRequest* cr) {
  if (cr->bucket->queued().current() < queueLimit()) {
    cr->backend = nullptr;
    cr->bucket->enqueue(cr);
    ++queued_;

    TRACE("HTTP cluster $0 [$1] overloaded. Enqueueing request ($2).",
          name(),
          cr->bucket->name(),
          cr->bucket->queued().current());

    return true;
  }

  TRACE("director: '$0' queue limit $1 reached.", name(), queueLimit());

  serviceUnavailable(cr);

  return false;
}

/**
 * Pops an enqueued request from the front of the queue and passes it to the
 * backend for serving.
 *
 * @param backend the backend to pass the dequeued request to.
 */
void HttpCluster::dequeueTo(HttpClusterMember* backend) {
  if (auto cr = dequeue()) {
    cr->post([this, backend, cr]() {
      cr->tokens = 1;
      TRACE("Dequeueing request to backend $0 @ $1", backend->name(), name());
      HttpClusterSchedulerStatus rc = backend->tryProcess(cr);
      if (rc != HttpClusterSchedulerStatus::Success) {
        cr->tokens = 0;
        static const char* ss[] = {"Unavailable.", "Success.", "Overloaded."};
        logError("HttpCluster",
                 "Dequeueing request to backend $0 @ $1 failed. $2",
                 backend->name(), name(), ss[(size_t)rc]);
        reschedule(cr);
      } else {
        // FIXME: really here????
        verifyTryCount(cr);
      }
    });
  } else {
    TRACE("dequeueTo: queue empty.");
  }
}

HttpClusterRequest* HttpCluster::dequeue() {
  if (auto cr = shaper()->dequeue()) {
    --queued_;
    TRACE("Director $0 dequeued request ($1 pending).", name(), queued_.current());
    return cr;
  }
  TRACE("Director $0 dequeue() failed ($1 pending).", name(), queued_.current());

  return nullptr;
}

void HttpCluster::onTimeout(HttpClusterRequest* cr) {
  --queued_;

  cr->post([this, cr]() {
    Duration diff = MonotonicClock::now() - cr->ctime;
    logInfo("HttpCluster",
            "Queued request timed out ($0). $1 $2",
            diff,
            cr->requestInfo.method(),
            cr->requestInfo.path());

    serviceUnavailable(cr, HttpStatus::GatewayTimeout);
  });
}
} // namespace client
} // namespace http
} // namespace xzero
