/*
 * File: task.cc
 * Project: libarc
 * File Created: Sunday, 20th December 2020 3:40:17 pm
 * Author: Minjun Xu (mjxu96@outlook.com)
 * -----
 * MIT License
 * Copyright (c) 2020 Minjun Xu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <arc/coro/task.h>

using namespace arc::coro;

void arc::coro::EnsureFuture(Task<void>&& task) { task.Start(true); }

void arc::coro::RunUntilComplete() {
  auto& event_loop = EventLoop::GetLocalInstance();
  event_loop.InitDo();
  while (!event_loop.IsDone()) {
    event_loop.Do();
  }
  event_loop.CleanUpFinishedCoroutines();
}

void arc::coro::StartEventLoop(Task<void>&& task) {
  task.Start(true);
  RunUntilComplete();
}

TimeAwaiter arc::coro::SleepFor(
    const std::chrono::steady_clock::duration& duration) {
  return TimeAwaiter(duration);
}

TimeAwaiter arc::coro::Yield() { return TimeAwaiter(std::chrono::seconds(0)); }
