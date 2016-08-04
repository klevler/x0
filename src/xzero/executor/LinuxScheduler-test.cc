// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/LinuxScheduler.h>
#include <xzero/io/SystemPipe.h>
#include <xzero/MonotonicTime.h>
#include <xzero/MonotonicClock.h>
#include <xzero/Application.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <memory>
#include <xzero/testing.h>

#include <fcntl.h>
#include <unistd.h>

using namespace xzero;

/* test case:
 * 1.) insert interest A with timeout 10s
 * 2.) after 5 seconds, insert interest B with timeout 2
 * 3.) the interest B should now be fired after 2 seconds
 * 4.) the interest A should now be fired after 3 seconds
 */
TEST(LinuxSchedulerTest, timeoutBreak) {
  LinuxScheduler scheduler;
  SystemPipe a;
  SystemPipe b;
  MonotonicTime start = MonotonicClock::now();
  MonotonicTime a_fired_at;
  MonotonicTime b_fired_at;
  MonotonicTime a_timeout_at;
  MonotonicTime b_timeout_at;
  auto a_fired = [&]() { a_fired_at = MonotonicClock::now();
                         logTrace("x", "a_fired_at: $0", a_fired_at); };
  auto b_fired = [&]() { b_fired_at = MonotonicClock::now();
                         logTrace("x", "b_fired_at: $0", b_fired_at); };
  auto a_timeout = [&]() { a_timeout_at = MonotonicClock::now();
                           logTrace("x", "a_timeout_at: $0", a_timeout_at - start); };
  auto b_timeout = [&]() { b_timeout_at = MonotonicClock::now();
                           logTrace("x", "b_timeout_at: $0", b_timeout_at - start); };

  scheduler.executeOnReadable(a.readerFd(), a_fired,
                              500_milliseconds, a_timeout);
  scheduler.executeOnReadable(b.readerFd(), b_fired,
                              100_milliseconds, b_timeout);

  scheduler.runLoop();

  EXPECT_TRUE(!a_fired_at);
  EXPECT_TRUE(!b_fired_at);
  EXPECT_NEAR(500, (a_timeout_at - start).milliseconds(), 50);
  EXPECT_NEAR(100,  (b_timeout_at - start).milliseconds(), 50);
}

TEST(LinuxSchedulerTest, executeAfter_without_handle) {
  LinuxScheduler scheduler;
  MonotonicTime start;
  MonotonicTime firedAt;
  int fireCount = 0;

  scheduler.executeAfter(50_milliseconds, [&](){
    firedAt = MonotonicClock::now();
    fireCount++;
  });

  start = MonotonicClock::now();
  firedAt = start;

  scheduler.runLoopOnce();

  Duration diff = firedAt - start;

  EXPECT_EQ(1, fireCount);
  EXPECT_NEAR(50, diff.milliseconds(), 10);
}

TEST(LinuxSchedulerTest, executeAfter_cancel_beforeRun) {
  LinuxScheduler scheduler;
  int fireCount = 0;

  auto handle = scheduler.executeAfter(1_seconds, [&](){
    printf("****** cancel_beforeRun: running action\n");
    fireCount++;
  });

  EXPECT_EQ(1, scheduler.referenceCount());
  handle->cancel();
  EXPECT_EQ(0, scheduler.referenceCount());
  EXPECT_EQ(0, fireCount);
}

TEST(LinuxSchedulerTest, executeAfter_cancel_beforeRun2) {
  LinuxScheduler scheduler;
  int fire1Count = 0;
  int fire2Count = 0;

  auto handle1 = scheduler.executeAfter(1_seconds, [&](){
    fire1Count++;
  });

  auto handle2 = scheduler.executeAfter(10_milliseconds, [&](){
    fire2Count++;
  });

  EXPECT_EQ(2, scheduler.referenceCount());
  handle1->cancel();
  EXPECT_EQ(1, scheduler.referenceCount());

  scheduler.runLoopOnce();

  EXPECT_EQ(0, fire1Count);
  EXPECT_EQ(1, fire2Count);
}

TEST(LinuxSchedulerTest, executeOnReadable) {
  // executeOnReadable: test cancellation after fire
  // executeOnReadable: test fire
  // executeOnReadable: test timeout
  // executeOnReadable: test fire at the time of the timeout

  LinuxScheduler sched;

  SystemPipe pipe;
  int fireCount = 0;
  int timeoutCount = 0;

  pipe.write("blurb");

  auto handle = sched.executeOnReadable(
      pipe.readerFd(),
      [&] { fireCount++; },
      Duration::Zero,
      [&] { timeoutCount++; } );

  EXPECT_EQ(0, fireCount);
  EXPECT_EQ(0, timeoutCount);

  sched.runLoopOnce();

  EXPECT_EQ(1, fireCount);
  EXPECT_EQ(0, timeoutCount);
}

TEST(LinuxSchedulerTest, executeOnReadable_timeout) {
  LinuxScheduler sched;
  SystemPipe pipe;

  int fireCount = 0;
  int timeoutCount = 0;
  auto onFire = [&] { fireCount++; };
  auto onTimeout = [&] { timeoutCount++; };

  sched.executeOnReadable(pipe.readerFd(), onFire, 500_milliseconds, onTimeout);
  sched.runLoop();

  EXPECT_EQ(0, fireCount);
  EXPECT_EQ(1, timeoutCount);
}

TEST(LinuxSchedulerTest, executeOnReadable_timeout_on_cancelled) {
  LinuxScheduler sched;
  SystemPipe pipe;

  int fireCount = 0;
  int timeoutCount = 0;
  auto onFire = [&] { fireCount++; };
  auto onTimeout = [&] {
    printf("onTimeout!\n");
    timeoutCount++; };

  auto handle = sched.executeOnReadable(
      pipe.readerFd(), onFire, 500_milliseconds, onTimeout);

  handle->cancel();
  sched.runLoopOnce();

  EXPECT_EQ(0, fireCount);
  EXPECT_EQ(0, timeoutCount);
}

// class AlreadyWatchingOnResource : public RuntimeError {
// public:
//   AlreadyWatchingOnResource()
//       : RuntimeError("Already watching on resource") {}
// };
// 
TEST(LinuxSchedulerTest, executeOnReadable_twice_on_same_fd) {
  LinuxScheduler sched;
  SystemPipe pipe;

  sched.executeOnReadable(pipe.readerFd(), [] () {});

  // FIXME
  // EXPECT_EXCEPTION("Already watching on resource", [&]() {
  //   sched.executeOnReadable(pipe.readerFd(), [] () {});
  // });

  // FIXME
  // EXPECT_THROW_STATUS(AlreadyWatchingOnResource,
  //     [&]() { sched.executeOnReadable(pipe.readerFd(), [] () {}); });
}

TEST(LinuxSchedulerTest, executeOnWritable) {
  LinuxScheduler sched;

  SystemPipe pipe;
  int fireCount = 0;
  int timeoutCount = 0;
  const Duration timeout = 1_seconds;
  const auto onFire = [&]() { fireCount++; };
  const auto onTimeout = [&]() { timeoutCount++; };

  sched.executeOnWritable(pipe.writerFd(), onFire, timeout, onTimeout);

  EXPECT_EQ(0, fireCount);
  EXPECT_EQ(0, timeoutCount);

  sched.runLoopOnce();

  EXPECT_EQ(1, fireCount);
  EXPECT_EQ(0, timeoutCount);
}

// TEST(LinuxSchedulerTest, waitForReadable) {
// };
// 
// TEST(LinuxSchedulerTest, waitForWritable) {
// };
// 
// TEST(LinuxSchedulerTest, waitForReadable_timed) {
// };
// 
// TEST(LinuxSchedulerTest, waitForWritable_timed) {
// };
