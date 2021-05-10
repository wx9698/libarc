/*
 * File: tls_socket.h
 * Project: libarc
 * File Created: Sunday, 9th May 2021 9:21:38 am
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

#ifndef LIBARC__IO__TLS_SOCKET_H
#define LIBARC__IO__TLS_SOCKET_H

#include "socket.h"

namespace arc {
namespace io {

template <net::Domain AF = net::Domain::IPV4, Pattern PP = Pattern::SYNC>
class TLSSocket : virtual public Socket<AF, net::Protocol::TCP, PP> {
 public:
  TLSSocket(TLSProtocol protocol = TLSProtocol::NOT_SPEC,
            TLSProtocolType type = TLSProtocolType::CLIENT)
      : Socket<AF, net::Protocol::TCP, PP>(),
        protocol_(protocol),
        type_(type),
        context_ptr_(&GetLocalSSLContext(protocol, type)) {
    ssl_ = context_ptr_->FetchSSL();
    BindFdWithSSL();
  }

  TLSSocket(Socket<AF, net::Protocol::TCP, PP>&& other, TLSProtocol protocol,
            TLSProtocolType type)
      : Socket<AF, net::Protocol::TCP, PP>(std::move(other)),
        protocol_(protocol),
        type_(type),
        context_ptr_(&GetLocalSSLContext(protocol, type)) {
    ssl_ = context_ptr_->FetchSSL();
    BindFdWithSSL();
  }

  virtual ~TLSSocket() {
    if (context_ptr_) {
      context_ptr_->FreeSSL(ssl_);
    }
  }
  TLSSocket(int fd, const net::Address<AF>& in_addr,
            TLSProtocol protocol = TLSProtocol::NOT_SPEC,
            TLSProtocolType type = TLSProtocolType::CLIENT)
      : Socket<AF, net::Protocol::TCP, PP>(fd, in_addr),
        protocol_(protocol),
        type_(type),
        context_ptr_(&GetLocalSSLContext(protocol, type)) {
    ssl_ = context_ptr_->FetchSSL();
    BindFdWithSSL();
  }

  TLSSocket(const TLSSocket&) = delete;
  TLSSocket& operator=(const TLSSocket&) = delete;
  TLSSocket(TLSSocket&& other)
      : Socket<AF, net::Protocol::TCP, PP>(std::move(other)),
        protocol_(other.protocol_),
        type_(other.type_),
        context_ptr_(other.context_ptr_),
        ssl_(other.ssl_) {
    other.ssl_.ssl = nullptr;
    other.context_ptr_ = nullptr;
    BindFdWithSSL();
  }

  TLSSocket& operator=(TLSSocket&& other) {
    Socket<AF, net::Protocol::TCP, PP>::operator=(std::move(other));
    if (ssl_.ssl && context_ptr_) {
      context_ptr_->FreeSSL(ssl_);
    }
    protocol_ = other.protocol_;
    type_ = other.type_;
    context_ptr_ = other.context_ptr_;
    ssl_ = other.ssl_;
    other.ssl_.ssl = nullptr;
    other.context_ptr_ = nullptr;
    BindFdWithSSL();
    return *this;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) ssize_t Recv(char* buf, int max_recv_bytes) {
    ssize_t tmp_read = SSL_read(ssl_.ssl, buf, max_recv_bytes);
    if (tmp_read == -1) {
      throw arc::exception::TLSException("Read Error");
    }
    return tmp_read;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) std::string RecvAll(int max_recv_bytes = -1) {
    std::string buffer;
    max_recv_bytes = (max_recv_bytes < 0
                          ? std::numeric_limits<decltype(max_recv_bytes)>::max()
                          : max_recv_bytes);
    int left_size = max_recv_bytes;
    while (left_size > 0) {
      int this_read_size = std::min(left_size, RECV_BUFFER_SIZE);
      ssize_t tmp_read = SSL_read(ssl_.ssl, this->buffer_, this_read_size);
      if (tmp_read > 0) {
        buffer.append(this->buffer_, tmp_read);
      }
      if (tmp_read == -1) {
        throw arc::exception::TLSException("Read Error");
      } else if (tmp_read < RECV_BUFFER_SIZE) {
        // we read all contents;
        break;
      }
      left_size -= tmp_read;
    }
    return buffer;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<ssize_t> Recv(char* buf,
                                                           int max_recv_bytes) {
    while (true) {
      ssize_t ret = SSL_read(ssl_.ssl, buf, RECV_BUFFER_SIZE);
      if (ret <= 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::READ);
        } else if (err == SSL_ERROR_WANT_WRITE) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::WRITE);
        } else if (err == SSL_ERROR_ZERO_RETURN) {
          co_return 0;
        } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
          co_return 0;
        } else {
          throw exception::TLSException("Read Error");
        }
      } else {
        co_return ret;
      }
    }
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<std::string> RecvAll(
      int max_recv_bytes = -1) {
    if (max_recv_bytes >= 0) {
      throw std::logic_error(
          "Async Recv of TLSSocket cannot specify the max_recv_bytes.");
    }
    int ret = -1;
    bool is_data_read = false;
    std::string ret_str;
    while (true) {
      ret = SSL_read(ssl_.ssl, this->buffer_, RECV_BUFFER_SIZE);
      if (ret <= 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          if (!is_data_read) {
            co_await coro::IOAwaiter(
                std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
                std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
                this->fd_, arc::io::IOType::READ);
          } else {
            break;
          }
        } else if (err == SSL_ERROR_WANT_WRITE) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::WRITE);
        } else if (err == SSL_ERROR_ZERO_RETURN) {
          // no data, we exit
          break;
        } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
          break;
        } else {
          throw exception::TLSException("Read Error");
        }
      } else {
        is_data_read = true;
        ret_str.append(this->buffer_, ret);
      }
    }
    co_return ret_str;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) int Send(const void* data, int num) {
    int writtern_size = SSL_write(ssl_.ssl, data, num);
    if (writtern_size < 0) {
      throw arc::exception::TLSException("Write Error");
    }
    return writtern_size;
  }

  template <Pattern UPP = PP, concepts::Writable DataType>
  requires(UPP == Pattern::SYNC) int Send(DataType&& data) {
    return Send(data.c_str(), data.size());
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<int> Send(const void* data,
                                                       int num) {
    co_await coro::IOAwaiter(
        std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
        std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this), this->fd_,
        arc::io::IOType::WRITE);
    int ret = -1;
    do {
      ret = SSL_write(ssl_.ssl, data, num);
      if (ret < 0) {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::READ);
        } else if (err == SSL_ERROR_WANT_WRITE) {
          co_await coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::WRITE);
        } else {
          throw exception::TLSException("Write Error", err);
        }
      }
    } while (ret < 0);
    co_return ret;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) void Connect(const net::Address<AF>& addr) {
    Socket<AF, net::Protocol::TCP, UPP>::Connect(addr);
    if (SSL_connect(ssl_.ssl) != 1) {
      throw arc::exception::TLSException("Connection Error");
    }
  }

  template <Pattern UPP = PP>
  requires(UPP ==
           Pattern::ASYNC) coro::Task<void> Connect(net::Address<AF> addr) {
    // initial TCP connect
    co_await Socket<AF, net::Protocol::TCP, UPP>::Connect(addr);

    // then TLS handshakes
    SSL_set_connect_state(ssl_.ssl);
    int r = 0;
    while ((r = HandShake()) != 1) {
      int err = SSL_get_error(ssl_.ssl, r);
      if (err == SSL_ERROR_WANT_WRITE) {
        co_await arc::coro::IOAwaiter(
            std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
            std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this), this->fd_,
            arc::io::IOType::WRITE);
      } else if (err == SSL_ERROR_WANT_READ) {
        co_await arc::coro::IOAwaiter(
            std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
            std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this), this->fd_,
            arc::io::IOType::READ);
      } else {
        throw arc::exception::TLSException("Connection Error");
      }
    }
    co_return;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) void Shutdown() {
    if (!ssl_.ssl) {
      return;
    }
    int ret = 0;
    while (true) {
      ret = SSL_shutdown(ssl_.ssl);
      if (ret == 1) {
        // successfully shutdown
        break;
      } else if (ret == 0) {
        // not finished yet, call again
        continue;
      } else {
        int ssl_err = SSL_get_error(ssl_.ssl, ret);
        throw arc::exception::TLSException("Shutdown Error", ssl_err);
      }
    }
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<void> Shutdown() {
    if (!ssl_.ssl) {
      co_return;
    }
    int ret = 0;
    while (true) {
      ret = SSL_shutdown(ssl_.ssl);
      if (ret == 1) {
        // successfully shutdown
        break;
      } else if (ret == 0) {
        // not finished yet, call again
        continue;
      } else {
        int err = SSL_get_error(ssl_.ssl, ret);
        if (err == SSL_ERROR_WANT_WRITE) {
          co_await arc::coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::WRITE);
        } else if (err == SSL_ERROR_WANT_READ) {
          co_await arc::coro::IOAwaiter(
              std::bind(&TLSSocket<AF, PP>::TLSIOReadyFunctor, this),
              std::bind(&TLSSocket<AF, PP>::TLSIOResumeFunctor, this),
              this->fd_, arc::io::IOType::READ);
        } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
          break;
        } else {
          throw arc::exception::TLSException("Shutdown Error", err);
        }
      }
    }
    co_return;
  }

  void SetAcceptState() { SSL_set_accept_state(ssl_.ssl); }

  io::SSL& GetSSLObject() { return ssl_; }

  int HandShake() { return SSL_do_handshake(ssl_.ssl); }

 protected:
  bool TLSIOReadyFunctor() { return false; }
  void TLSIOResumeFunctor() { return; }

  void BindFdWithSSL() { SSL_set_fd(ssl_.ssl, this->fd_); }

  io::SSLContext* context_ptr_{nullptr};
  io::SSL ssl_{};
  TLSProtocol protocol_;
  TLSProtocolType type_;
};

template <net::Domain AF = net::Domain::IPV4, Pattern PP = Pattern::SYNC>
class TLSAcceptor : public TLSSocket<AF, PP>, public Acceptor<AF, PP> {
 public:
  using TLSSocket<AF, PP>::Connect;
  using TLSSocket<AF, PP>::Send;
  using TLSSocket<AF, PP>::Recv;

  TLSAcceptor(const std::string& cert_file, const std::string& key_file,
              TLSProtocol protocol = TLSProtocol::NOT_SPEC,
              TLSProtocolType type = TLSProtocolType::SERVER)
      : TLSSocket<AF, PP>(protocol, type),
        Acceptor<AF, PP>(),
        cert_file_(cert_file),
        key_file_(key_file) {
    LoadCertificateAndKey(cert_file_, key_file_);
  }
  TLSAcceptor(TLSAcceptor&& other)
      : TLSSocket<AF, PP>(std::move(other)),
        Acceptor<AF, PP>(std::move(other)),
        cert_file_(std::move(other.cert_file_)),
        key_file_(std::move(other.key_file_)) {
    LoadCertificateAndKey(cert_file_, key_file_);
  }
  TLSAcceptor& operator=(TLSAcceptor&& other) {
    cert_file_ = std::move(other.cert_file_);
    key_file_ = std::move(other.key_file_);
    TLSSocket<AF, PP>::operator=(std::move(other));
    Acceptor<AF, PP>::operator=(std::move(other));
    LoadCertificateAndKey(cert_file_, key_file_);
    return *this;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::SYNC) TLSSocket<AF, PP> Accept() {
    TLSSocket<AF, PP> tls_socket(Acceptor<AF, PP>::InternalAccept(),
                                 this->protocol_, this->type_);
    tls_socket.SetAcceptState();
    if (tls_socket.HandShake() != 1) {
      throw arc::exception::TLSException("Accept Error");
    }
    return tls_socket;
  }

  template <Pattern UPP = PP>
  requires(UPP == Pattern::ASYNC) coro::Task<TLSSocket<AF, PP>> Accept() {
    TLSSocket<AF, PP> tls_socket(co_await Acceptor<AF, PP>::Accept(),
                                 this->protocol_, this->type_);
    tls_socket.SetAcceptState();
    int r = 0;
    co_await arc::coro::IOAwaiter(
        std::bind(&TLSAcceptor<AF, PP>::TLSIOReadyFunctor, this),
        std::bind(&TLSAcceptor<AF, PP>::TLSIOResumeFunctor, this),
        tls_socket.GetFd(), arc::io::IOType::READ);
    while ((r = tls_socket.HandShake()) != 1) {
      int err = SSL_get_error(tls_socket.GetSSLObject().ssl, r);
      if (err == SSL_ERROR_WANT_WRITE) {
        co_await arc::coro::IOAwaiter(
            std::bind(&TLSAcceptor<AF, PP>::TLSIOReadyFunctor, this),
            std::bind(&TLSAcceptor<AF, PP>::TLSIOResumeFunctor, this),
            tls_socket.GetFd(), arc::io::IOType::WRITE);
      } else if (err == SSL_ERROR_WANT_READ) {
        co_await arc::coro::IOAwaiter(
            std::bind(&TLSAcceptor<AF, PP>::TLSIOReadyFunctor, this),
            std::bind(&TLSAcceptor<AF, PP>::TLSIOResumeFunctor, this),
            tls_socket.GetFd(), arc::io::IOType::READ);
      } else if (err == SSL_ERROR_SYSCALL && errno == 0) {
        break;
      } else {
        throw arc::exception::TLSException("Accept Error", err);
      }
    }
    co_return std::move(tls_socket);
  }

 private:
  bool TLSIOReadyFunctor() { return false; }
  void TLSIOResumeFunctor() { return; }

  void LoadCertificateAndKey(const std::string& cert_file,
                             const std::string& key_file) {
    this->context_ptr_->SetCertificateAndKey(cert_file, key_file);
  }

  std::string cert_file_;
  std::string key_file_;
};

}  // namespace io

}  // namespace arc

#endif