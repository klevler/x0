// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/UnixTime.h>
#include <xzero/Duration.h>
#include <xzero/io/File.h>
#include <xzero/CustomDataMgr.h>
#include <xzero-flow/vm/Params.h>
#include <xzero-flow/vm/Runner.h>
#include <string>
#include <list>
#include <vector>
#include <functional>
#include <memory>

namespace xzero {
  class UnixTime;
  namespace http {
    class HttpRequest;
    class HttpResponse;
  }
}

namespace x0d {

/**
 * HTTP client context.
 *
 * Contains all the necessary references to everything you (may) need
 * during request handling.
 */
class XzeroContext {
  CUSTOMDATA_API_INLINE
 public:
  XzeroContext(
      std::shared_ptr<xzero::flow::vm::Handler> entrypoint,
      xzero::http::HttpRequest* request,
      xzero::http::HttpResponse* response);

  xzero::http::HttpRequest* request() const noexcept { return request_; }
  xzero::http::HttpResponse* response() const noexcept { return response_; }

  xzero::UnixTime createdAt() const { return createdAt_; }
  xzero::UnixTime now() const;
  xzero::Duration duration() const;

  const std::string& documentRoot() const noexcept { return documentRoot_; }
  void setDocumentRoot(const std::string& path) { documentRoot_ = path; }

  const std::string& pathInfo() const noexcept { return pathInfo_; }
  void setPathInfo(const std::string& value) { pathInfo_ = value; }

  void setFile(std::shared_ptr<xzero::File> file) { file_ = file; }
  std::shared_ptr<xzero::File> file() const { return file_; }

  xzero::flow::vm::Runner* runner() const noexcept { return runner_.get(); }

  void run();

  const xzero::IPAddress& remoteIP() const;
  int remotePort() const;

  const xzero::IPAddress& localIP() const;
  int localPort() const;

  size_t bytesReceived() const;
  size_t bytesTransmitted() const;

  bool verifyDirectoryDepth();

  void setErrorHandler(std::shared_ptr<xzero::flow::vm::Handler> eh) {
    errorHandler_ = eh;
  }

  bool invokeErrorHandler() {
    if (errorHandler_) {
      return errorHandler_->run(this);
    } else {
      return false;
    }
  }

 private:
  std::unique_ptr<xzero::flow::vm::Runner> runner_; //!< Flow VM execution unit.
  xzero::UnixTime createdAt_; //!< When the request started
  xzero::http::HttpRequest* request_; //!< HTTP request
  xzero::http::HttpResponse* response_; //!< HTTP response
  std::string documentRoot_; //!< associated document root
  std::string pathInfo_; //!< info-part of the request-path
  std::shared_ptr<xzero::File> file_; //!< local file associated with this request
  std::shared_ptr<xzero::flow::vm::Handler> errorHandler_; //!< custom error handler
};

} // namespace x0d
