/*
 * File: coro_socket_example.cc
 * Project: libarc
 * File Created: Sunday, 13th December 2020 6:01:29 pm
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
#include <arc/io/socket.h>
#include <thread>

using namespace arc::io;
using namespace arc::net;
using namespace arc::coro;

const std::string request =
    "GET / HTTP/1.1\r\nHost: www.google.com\r\nUser-Agent: "
    "curl/7.58.0\r\nAccept: */*\r\n\r\n";
const std::string ret =
    "HTTP/1.1 200 OK\r\nContent-Length: 140\r\nContent-Type: "
    "text/"
    "plain\r\n\r\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaa";


Task<void> LongTimeTask() {
  co_await SleepFor(std::chrono::seconds(86400));
  std::cout << "long time task finishes" << std::endl;
}

void StartLongTimeTask() {
  std::cout << std::this_thread::get_id() << " creates long time job" << std::endl;
  GetEventLoop().StartLoop(LongTimeTask());
}


Task<void> HandleClient(
    Socket<Domain::IPV4, Protocol::TCP, Pattern::ASYNC> sock) {
  try {
    while (true) {
      auto recv = co_await sock.Recv();
      if (recv.size() == 0) {
        break;
      }
      // std::cout << "recv " << recv << std::endl;
      // std::cout << std::this_thread::get_id() << " before sleep" << std::endl;
      // co_await SleepFor(std::chrono::seconds(2));
      // std::cout << std::this_thread::get_id() << " after sleep" << std::endl;
      // std::string ret_in_stack = std::string(ret.c_str(), ret.size());
      co_await sock.Send(ret.c_str(), ret.size());
      // std::cout << "sent" << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }
}

Task<void> HandleClient(
    TLSSocket<Domain::IPV4, Pattern::ASYNC> sock) {
  try {
    while (true) {
      auto recv = co_await sock.Recv();
      if (recv.size() == 0) {
        break;
      }
      co_await sock.Send(ret.c_str(), ret.size());
    }
    // co_await sock.Shutdown();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }
}

Task<void> Listen(int thread_num) {
  Acceptor<Domain::IPV4, Pattern::ASYNC> accpetor;
  std::cout << "acceptor fd: " << accpetor.GetFd() << std::endl;
  accpetor.SetOption(arc::net::SocketOption::REUSEADDR, 1);
  // accpetor.SetOption(arc::net::SocketOption::REUSEPORT, 1);
  uint16_t port = 8086;
  accpetor.Bind({"localhost", port});
  accpetor.Listen();
  std::vector<std::thread> threads;
  auto this_thread_id = std::this_thread::get_id();
  std::cout << "listen thread: " << this_thread_id << std::endl;
  for (int i = 0; i < thread_num; i++) {
    threads.emplace_back(StartLongTimeTask);
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::cout << "http listen starts at port: " << port << std::endl;
  auto itr = GetAllEventLoops().begin();
  int i = 0;
  while (i < 3) {
    if (itr->first == this_thread_id) {
      itr++;
    }
    if (itr == GetAllEventLoops().end()) {
      itr = GetAllEventLoops().begin();
    }
    assert(itr->first != this_thread_id);
    auto in_sock = co_await accpetor.Accept();
    std::cout << "receive one client" << std::endl;
    // i++;
    itr->second->EnsureFuture(HandleClient(std::move(in_sock)));

    // arc::coro::EnsureFuture(HandleClient(std::move(in_sock)));
    // i++;
    // std::cout << "thread: 0x" << std::hex << itr->first
    //           << std::dec << " handled " << i << " clients" << std::endl;
  }
  std::cout << "wait join" << std::endl;
  for (int i = 0; i < thread_num; i++) {
    threads[i].join();
  }

  co_return;
}

Task<void> Connect() {
  Socket<arc::net::Domain::IPV4, arc::net::Protocol::TCP,
         arc::io::Pattern::ASYNC>
      client;
  // co_await client.Connect({"ip6-localhost", 8080});
  std::cout << "start async http connect" << std::endl;
  co_await client.Connect({"www.google.com", 80});
  std::cout << "before send" << std::endl;
  co_await client.Send(request.c_str(), request.size());
  std::cout << "after send" << std::endl;
  std::cout << co_await client.Recv() << std::endl;
  co_return;
}

Task<void> TLSConnect() {
  TLSSocket<Domain::IPV4, Pattern::ASYNC> tls_socket;
  std::cout << "start async https connect" << std::endl;
  co_await tls_socket.Connect({"www.google.com", 443});
  std::cout << "before send" << std::endl;
  co_await tls_socket.Send(request.c_str(), request.size());
  std::cout << "after send" << std::endl;
  std::cout << co_await tls_socket.Recv() << std::endl;
  co_return;
}

Task<void> TLSAccept() {
  TLSAcceptor<Domain::IPV4, Pattern::ASYNC> accpetor("/home/minjun/data/keys/server.pem", "/home/minjun/data/keys/server.pem");
  std::cout << "acceptor fd: " << accpetor.GetFd() << std::endl;
  accpetor.SetOption(arc::net::SocketOption::REUSEADDR, 1);
  accpetor.SetOption(arc::net::SocketOption::REUSEPORT, 1);
  uint16_t port = 8086;
  accpetor.Bind({"localhost", port});
  accpetor.Listen();
  std::cout << "https listen starts at port " << port << std::endl;
  int i = 0;
  while (i < 2) {
    auto in_sock = co_await accpetor.Accept();
    // EnsureFuture(HandleClient(std::move(in_sock)));
  }
}

void Start(int thread_num) { GetEventLoop().StartLoop(Listen(thread_num)); }

void TLSStart() { GetEventLoop().StartLoop(TLSAccept()); }

int main(int argc, char** argv) {
  // StartEventLoop(Connect());
  // std::vector<std::thread> threads;
  Start(std::stoi(std::string(argv[1])));
  // int thread_num = std::stoi(std::string(argv[1]));
  // for (int i = 0; i < thread_num; i++) {
  //   threads.emplace_back(Start, thread_num);
  // }

  // for (int i = 0; i < thread_num; i++) {
  //   threads[i].join();
  // }

  std::cout << "finished" << std::endl;
  // StartEventLoop(TLSAccept());
  // StartEventLoop(TLSConnect());
  // StartEventLoop(Listen());
  // StartEventLoop(Connect());
}
