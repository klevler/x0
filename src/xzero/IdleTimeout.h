// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/Duration.h>
#include <xzero/MonotonicTime.h>
#include <xzero/executor/Executor.h>
#include <functional>

namespace xzero {

/**
 * Manages a single idle timeout.
 */
class XZERO_BASE_API IdleTimeout {
 public:
  IdleTimeout(Executor* executor);
  ~IdleTimeout();

  void setTimeout(Duration value);
  Duration timeout() const;

  void setCallback(std::function<void()>&& cb);
  void clearCallback();

  void activate();
  void activate(Duration timeout);
  void deactivate();
  bool isActive() const;

  /**
   * Resets idle timer.
   *
   * Touches the idle-timeout object, effectively resetting the timer back to 0.
   *
   * If this object is not activated nothing will happen.
   */
  void touch();

  /**
   * Retrieves the timespan elapsed since idle timer started or 0 if inactive.
   */
  Duration elapsed() const;

 private:
  void schedule();
  void reschedule();
  void onFired();

 private:
  Executor* executor_;
  Duration timeout_;
  MonotonicTime fired_;
  bool active_;
  std::function<void()> onTimeout_;
  Executor::HandleRef handle_;
};

inline void IdleTimeout::activate(Duration timeout) {
  setTimeout(timeout);
  activate();
}

} // namespace xzero
