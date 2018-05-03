/*
 *
 * Copyright 2017 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "asylo/platform/posix/sockets/socket_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <memory>

#include "asylo/util/logging.h"
#include "asylo/util/posix_error_space.h"

namespace asylo {
namespace {

#ifdef __ASYLO__
constexpr const char kLogOrigin[] = "WITHIN ENCLAVE: ";
#else
constexpr const char kLogOrigin[] = "OUTSIDE ENCLAVE: ";
#endif

}  // namespace

SocketClient::SocketClient() : connection_fd_(-1) {}

Status SocketClient::Read(void *buf, size_t read_len) {
  return sock_transmit_.Read(connection_fd_, buf, read_len);
}

Status SocketClient::Write(const void *buf, size_t write_len) {
  return sock_transmit_.Write(connection_fd_, buf, write_len);
}

Status SocketClient::RecvMsg(struct msghdr *msg, int flags) {
  return sock_transmit_.RecvMsg(connection_fd_, msg, flags);
}

Status SocketClient::SendMsg(const struct msghdr *msg, int flags) {
  return sock_transmit_.SendMsg(connection_fd_, msg, flags);
}

Status SocketClient::ClientSetup(const std::string &server_ip, int server_port) {
  connection_fd_ = socket(AF_INET6, SOCK_STREAM, 0);
  if (connection_fd_ < 0) {
    LOG(ERROR) << kLogOrigin << "client socket error";
    return Status(static_cast<error::PosixError>(errno), "socket error");
  }

  fd_closer_.reset(connection_fd_);

  struct sockaddr_in6 serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin6_family = AF_INET6;
  serv_addr.sin6_flowinfo = 0;
  serv_addr.sin6_port = htons(server_port);
  if (inet_pton(AF_INET6, server_ip.c_str(), &serv_addr.sin6_addr) <= 0) {
    LOG(ERROR) << kLogOrigin << "client inet_pton error";
    return Status(static_cast<error::PosixError>(errno), "inet_pton error");
  }

  Status status = ClientConnection(
      connection_fd_, reinterpret_cast<struct sockaddr *>(&serv_addr),
      sizeof(serv_addr));
  return status;
}

Status SocketClient::ClientSetup(const std::string &socket_name) {
  connection_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (connection_fd_ < 0) {
    LOG(ERROR) << kLogOrigin << "client socket error";
    return Status(static_cast<error::PosixError>(errno), "socket error");
  }

  fd_closer_.reset(connection_fd_);

  struct sockaddr_un serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path, socket_name.c_str(),
          sizeof(serv_addr.sun_path) - 1);

  Status status = ClientConnection(
      connection_fd_, reinterpret_cast<struct sockaddr *>(&serv_addr),
      sizeof(serv_addr));
  return status;
}

Status SocketClient::ClientRoundtripTransmit(int buf_len, int round_trip) {
  Status status = Status::OkStatus();
  std::unique_ptr<char[]> transmit_buf(new char[buf_len]);
  char *buf = transmit_buf.get();

  // In all roundtrips, the client keeps reading and writing garbage data.
  // It is intentional here because in our perf test what is stored in buf
  // does not matter.
  while (round_trip--) {
    if (!(status = Read(buf, buf_len)).ok()) {
      break;
    }
    if (!(status = Write(buf, buf_len)).ok()) {
      break;
    }
  }
  return status;
}

void SocketClient::LogClientIOStats() {
  LOG(INFO) << kLogOrigin << "client made " << sock_transmit_.GetWrite()
            << " calls to write";
  LOG(INFO) << kLogOrigin << "client made " << sock_transmit_.GetRead()
            << " calls to read";
}

Status SocketClient::ClientConnection(int fd, struct sockaddr *serv_addr,
                                      socklen_t addrlen) {
  if (connect(fd, serv_addr, addrlen) < 0) {
    LOG(ERROR) << kLogOrigin << "client connect timeout";
    return Status(static_cast<error::PosixError>(errno), "connect timeout");
  }
  return Status::OkStatus();
}

}  // namespace asylo
