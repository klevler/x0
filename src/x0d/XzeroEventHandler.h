// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroState.h>

namespace xzero {
  class HttpServer;
  class Executor;
  struct UnixSignalInfo;
}

namespace x0d {

class XzeroDaemon;

class XzeroEventHandler {
 public:
  XzeroEventHandler(XzeroDaemon* daemon, xzero::Executor* executor);
  ~XzeroEventHandler();

  xzero::Executor* executor() const { return executor_; }

  xzero::HttpServer* server() const;

  XzeroState state() const { return state_; }
  void setState(XzeroState newState);

 private:
  void onConfigReload();
  void onCycleLogs(const xzero::UnixSignalInfo& info);
  void onUpgradeBinary(const xzero::UnixSignalInfo& info);
  void onQuickShutdown();
  void onGracefulShutdown();

 private:
  XzeroDaemon* daemon_;
  xzero::Executor* executor_;
  XzeroState state_;
};

}  // namespace x0d

