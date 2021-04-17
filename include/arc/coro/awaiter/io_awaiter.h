/*
 * File: io_awaiter.h
 * Project: libarc
 * File Created: Sunday, 13th December 2020 3:58:54 pm
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

#ifndef LIBARC__CORO__AWAITER__IO_AWAITER_H
#define LIBARC__CORO__AWAITER__IO_AWAITER_H

#include <arc/coro/eventloop.h>
#include <arc/coro/events/io_event_base.h>
#include <arc/exception/io.h>
#include <arc/concept/coro.h>

#include <functional>

namespace arc {
namespace coro {

template <typename ReadyFunctorRetType, typename ResumeFunctorRetType,
          io::IOType T>
class [[nodiscard]] IOAwaiter {
 public:
  IOAwaiter(std::function<ReadyFunctorRetType()>&& ready_functor,
            std::function<ResumeFunctorRetType()>&& resume_functor, int fd)
      : ready_functor_(std::move(ready_functor)),
        resume_functor_(std::move(resume_functor)),
        fd_(fd) {}

  template <io::IOType UT = T>
  requires(UT == io::IOType::ACCEPT) bool await_ready() {
    return ready_functor_();
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::CONNECT) bool await_ready() {
    if (ready_functor_()) {
      return true;
    } else {
      if (errno == EINPROGRESS) {
        return false;
      }
    }
    throw arc::exception::IOException("Connection Error");
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::WRITE) bool await_ready() {
    written_size_ = ready_functor_();
    if (written_size_ < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return false;
      }
      throw arc::exception::IOException("Send Error");
    }
    return true;
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::READ) bool await_ready() {
    return false;
  }

  template <arc::concepts::PromiseT PromiseType>
  void await_suspend(std::coroutine_handle<PromiseType> handle) {
    GetLocalEventLoop().AddIOEvent(
        new events::detail::IOEventBase(fd_, T, handle));
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::ACCEPT) ResumeFunctorRetType await_resume() {
    return resume_functor_();
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::CONNECT) ResumeFunctorRetType await_resume() {
    return;
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::READ) ResumeFunctorRetType await_resume() {
    return resume_functor_();
  }

  template <io::IOType UT = T>
  requires(UT == io::IOType::WRITE) ResumeFunctorRetType await_resume() {
    if (written_size_ >= 0) [[likely]] {
      return written_size_;
    }
    return resume_functor_();
  }

 private:
  int written_size_{0};
  int fd_;

  std::function<ResumeFunctorRetType()> resume_functor_;
  std::function<ReadyFunctorRetType()> ready_functor_;
};

}  // namespace coro
}  // namespace arc

#endif /* LIBARC__CORO__AWAITER__IO_AWAITER_H */