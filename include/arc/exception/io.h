/*
 * File: io.h
 * Project: libarc
 * File Created: Wednesday, 14th April 2021 7:26:12 pm
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

#ifndef LIBARC__EXCEPTION__IO_H
#define LIBARC__EXCEPTION__IO_H

#include "base.h"
#include <openssl/ssl.h>

namespace arc {
namespace exception {

class IOException : public detail::ErrnoException {
 public:
#ifdef __clang__
  IOException(const std::string& msg = "");
#else
  IOException(const std::string& msg = "",
              const std::experimental::source_location& source_location =
                  std::experimental::source_location::current());
#endif
};

class TLSException : public detail::ExceptionBase {
 public:
#ifdef __clang__
  TLSException(const std::string& msg = "", int ssl_err = SSL_ERROR_SSL);
#else
  TLSException(const std::string& msg = "", int ssl_err = SSL_ERROR_SSL,
               const std::experimental::source_location& source_location =
                   std::experimental::source_location::current());
#endif
 private:
  int ssl_err_{0};
  std::string GetSSLError();
};

}  // namespace exception
}  // namespace arc

#endif