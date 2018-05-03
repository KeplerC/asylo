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

#include "asylo/platform/posix/sockets/socket_transmit.h"

#include <errno.h>
#include <unistd.h>

#include "asylo/util/posix_error_space.h"

namespace asylo {

SocketTransmit::SocketTransmit() : write_count_(0), read_count_(0) {}

Status SocketTransmit::Read(int fd, void *buf, size_t read_len) {
  int read_bytes = 0;
  int nbytes = 0;
  while (read_bytes < read_len) {
    nbytes = read(fd, reinterpret_cast<char *>(buf) + read_bytes,
                  read_len - read_bytes);
    if (nbytes < 0) {
      switch (errno) {
        case EINTR:
        case EAGAIN:
          continue;
        default:
          break;
      }
    } else if (!nbytes) {
      return Status(error::PosixError::P_EPIPE, "connection closed by peer");
    } else {
      read_bytes += nbytes;
      ++read_count_;
    }
  }
  if (read_bytes < read_len) {
    return Status(static_cast<error::PosixError>(errno), "read error");
  }
  return Status::OkStatus();
}

Status SocketTransmit::Write(int fd, const void *buf, size_t write_len) {
  int write_bytes = 0;
  int nbytes = 0;
  while (write_bytes < write_len) {
    nbytes = write(fd, reinterpret_cast<const char *>(buf) + write_bytes,
                   write_len - write_bytes);
    if (nbytes < 0) {
      switch (errno) {
        case EINTR:
        case EAGAIN:
          continue;
        default:
          break;
      }
    } else {
      write_bytes += nbytes;
      ++write_count_;
    }
  }
  if (write_bytes < write_len) {
    return Status(static_cast<error::PosixError>(errno), "write error");
  }
  return Status::OkStatus();
}

Status SocketTransmit::RecvMsg(int sockfd, struct msghdr *msg, int flags) {
  if (recvmsg(sockfd, msg, flags) == -1) {
    return Status(static_cast<error::PosixError>(errno),
                  std::string("recvmsg error:") + strerror(errno));
  }
  return Status::OkStatus();
}

Status SocketTransmit::SendMsg(int sockfd, const struct msghdr *msg,
                               int flags) {
  if (sendmsg(sockfd, msg, flags) == -1) {
    return Status(static_cast<error::PosixError>(errno),
                  std::string("sendmsg error:") + strerror(errno));
  }
  return Status::OkStatus();
}

int SocketTransmit::GetWrite() const { return write_count_; }

int SocketTransmit::GetRead() const { return read_count_; }

void SocketTransmit::reset() {
  write_count_ = 0;
  read_count_ = 0;
}

}  // namespace asylo
