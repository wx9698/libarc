/*
 * File: socket_example.cc
 * Project: libarc
 * File Created: Saturday, 12th December 2020 10:03:54 pm
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

#include <arc/io/socket.h>
using namespace arc::io;

void ConnectTest() {
  Socket sock;
  std::cout << "123" << std::endl;
  sock.Connect({"localhost", 8080});
  std::cout << sock.Send("123") << std::endl;
  std::cout << sock.Recv(3) << std::endl;
}

void AcceptTest() {
  Socket sock;
  sock.Bind({"localhost", 8081});
  sock.Listen();
  auto new_sock = sock.Accept();
  auto recv = new_sock.Recv();
  std::cout << recv << std::endl;
  std::cout << new_sock.Send(recv) << std::endl;
}

int main() {
  AcceptTest();
}
