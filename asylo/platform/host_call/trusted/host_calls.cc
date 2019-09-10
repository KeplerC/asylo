/*
 *
 * Copyright 2019 Asylo authors
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

#include "asylo/platform/host_call/trusted/host_calls.h"

#include <errno.h>
#include <sys/statfs.h>

#include "asylo/platform/host_call/exit_handler_constants.h"
#include "asylo/platform/host_call/trusted/host_call_dispatcher.h"
#include "asylo/platform/primitives/trusted_primitives.h"
#include "asylo/platform/system_call/type_conversions/types_functions.h"

template <class... Ts>
int64_t EnsureInitializedAndDispatchSyscall(int sysno, Ts... args) {
  if (!enc_is_syscall_dispatcher_set()) {
    enc_set_dispatch_syscall(asylo::host_call::SystemCallDispatcher);
  }
  return enc_untrusted_syscall(sysno, args...);
}

namespace {

size_t CalculateTotalSize(const struct msghdr *msg) {
  size_t total_message_size = 0;
  for (int i = 0; i < msg->msg_iovlen; ++i) {
    total_message_size += msg->msg_iov[i].iov_len;
  }
  return total_message_size;
}

}  // namespace

extern "C" {

int enc_untrusted_access(const char *path_name, int mode) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_access,
                                             path_name, mode);
}

pid_t enc_untrusted_getpid() {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_getpid);
}

pid_t enc_untrusted_getppid() {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_getppid);
}

pid_t enc_untrusted_setsid() {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_setsid);
}

uid_t enc_untrusted_getuid() {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_getuid);
}

gid_t enc_untrusted_getgid() {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_getgid);
}

uid_t enc_untrusted_geteuid() {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_geteuid);
}

gid_t enc_untrusted_getegid() {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_getegid);
}

int enc_untrusted_kill(pid_t pid, int sig) {
  int klinux_sig = TokLinuxSignalNumber(sig);
  if (klinux_sig < 0) {
    errno = EINVAL;
    return -1;
  }

  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_kill, pid,
                                             klinux_sig);
}

int enc_untrusted_link(const char *oldpath, const char *newpath) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_link,
                                             oldpath, newpath);
}

off_t enc_untrusted_lseek(int fd, off_t offset, int whence) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_lseek, fd,
                                             offset, whence);
}

int enc_untrusted_mkdir(const char *pathname, mode_t mode) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_mkdir,
                                             pathname, mode);
}

int enc_untrusted_open(const char *pathname, int flags, ...) {
  int mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_open, pathname, TokLinuxFileStatusFlag(flags),
      TokLinuxFileModeFlag(mode));
}

int enc_untrusted_unlink(const char *pathname) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_unlink,
                                             pathname);
}

int enc_untrusted_rename(const char *oldpath, const char *newpath) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_rename,
                                             oldpath, newpath);
}

ssize_t enc_untrusted_read(int fd, void *buf, size_t count) {
  return static_cast<ssize_t>(EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_read, fd, buf, count));
}

ssize_t enc_untrusted_write(int fd, const void *buf, size_t count) {
  return static_cast<ssize_t>(EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_write, fd, buf, count));
}

int enc_untrusted_symlink(const char *target, const char *linkpath) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_symlink,
                                             target, linkpath);
}

ssize_t enc_untrusted_readlink(const char *pathname, char *buf, size_t bufsiz) {
  return static_cast<ssize_t>(EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_readlink, pathname, buf, bufsiz));
}

int enc_untrusted_truncate(const char *path, off_t length) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_truncate,
                                             path, length);
}

int enc_untrusted_ftruncate(int fd, off_t length) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_ftruncate,
                                             fd, length);
}

int enc_untrusted_rmdir(const char *path) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_rmdir,
                                             path);
}

int enc_untrusted_pipe2(int pipefd[2], int flags) {
  if (flags & ~(O_CLOEXEC | O_DIRECT | O_NONBLOCK)) {
    errno = EINVAL;
    return -1;
  }

  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_pipe2, pipefd, TokLinuxFileStatusFlag(flags));
}

int enc_untrusted_socket(int domain, int type, int protocol) {
  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_socket, TokLinuxAfFamily(domain),
      TokLinuxSocketType(type), protocol);
}

int enc_untrusted_listen(int sockfd, int backlog) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_listen,
                                             sockfd, backlog);
}

int enc_untrusted_shutdown(int sockfd, int how) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_shutdown,
                                             sockfd, how);
}

ssize_t enc_untrusted_send(int sockfd, const void *buf, size_t len, int flags) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_sendto,
                                             sockfd, buf, len, flags,
                                             /*dest_addr=*/nullptr,
                                             /*addrlen=*/0);
}

int enc_untrusted_fcntl(int fd, int cmd, ... /* arg */) {
  // We do not currently support file locks in Asylo, so arg is not expected to
  // be a pointer to struct flock.
  int64_t arg = 0;
  va_list ap;
  va_start(ap, cmd);
  arg = va_arg(ap, int64_t);
  va_end(ap);

  int klinux_cmd = TokLinuxFcntlCommand(cmd);
  if (klinux_cmd == -1) {
    errno = EINVAL;
    return -1;
  }

  int intarg = arg;
  switch (cmd) {
    case F_SETFL: {
      return EnsureInitializedAndDispatchSyscall(
          asylo::system_call::kSYS_fcntl, fd, klinux_cmd,
          TokLinuxFileStatusFlag(intarg));
    }
    case F_SETFD: {
      return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_fcntl,
                                                 fd, klinux_cmd,
                                                 TokLinuxFDFlag(intarg));
    }
    case F_GETFL: {
      int retval = EnsureInitializedAndDispatchSyscall(
          asylo::system_call::kSYS_fcntl, fd, klinux_cmd, arg);
      if (retval != -1) {
        retval = FromkLinuxFileStatusFlag(retval);
      }

      return retval;
    }
    case F_GETFD: {
      int retval = EnsureInitializedAndDispatchSyscall(
          asylo::system_call::kSYS_fcntl, fd, klinux_cmd, arg);
      if (retval != -1) {
        retval = FromkLinuxFDFlag(retval);
      }
      return retval;
    }
    case F_GETPIPE_SZ:
    case F_SETPIPE_SZ: {
      return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_fcntl,
                                                 fd, klinux_cmd, arg);
    }
    // We do not handle the case for F_DUPFD. It is expected to be handled at
    // a higher abstraction, as we need not exit the enclave for duplicating
    // the file descriptor.
    default: {
      errno = EINVAL;
      return -1;
    }
  }
}

int enc_untrusted_chown(const char *pathname, uid_t owner, gid_t group) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_chown,
                                             pathname, owner, group);
}

int enc_untrusted_fchown(int fd, uid_t owner, gid_t group) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_fchown,
                                             fd, owner, group);
}

int enc_untrusted_setsockopt(int sockfd, int level, int optname,
                             const void *optval, socklen_t optlen) {
  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_setsockopt, sockfd, level,
      TokLinuxOptionName(level, optname), optval, optlen);
}

int enc_untrusted_flock(int fd, int operation) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_flock, fd,
                                             TokLinuxFLockOperation(operation));
}

int enc_untrusted_wait(int *wstatus) {
  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_wait4, /*wpid=*/-1, wstatus, /*options=*/0,
      /*rusage=*/nullptr);
}

int enc_untrusted_inotify_init1(int flags) {
  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_inotify_init1, TokLinuxInotifyFlag(flags));
}

int enc_untrusted_inotify_add_watch(int fd, const char *pathname,
                                    uint32_t mask) {
  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_inotify_add_watch, fd, pathname,
      TokLinuxInotifyEventMask(mask));
}

int enc_untrusted_inotify_rm_watch(int fd, int wd) {
  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_inotify_rm_watch, fd, wd);
}

mode_t enc_untrusted_umask(mode_t mask) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_umask,
                                             mask);
}

int enc_untrusted_chmod(const char *path_name, mode_t mode) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_chmod,
                                             path_name, mode);
}

int enc_untrusted_fchmod(int fd, mode_t mode) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_fchmod,
                                             fd, mode);
}

int enc_untrusted_sched_yield() {
  return EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_sched_yield);
}

int enc_untrusted_pread64(int fd, void *buf, size_t count, off_t offset) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_pread64,
                                             fd, buf, count, offset);
}

int enc_untrusted_pwrite64(int fd, const void *buf, size_t count,
                           off_t offset) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_pwrite64,
                                             fd, buf, count, offset);
}

int enc_untrusted_isatty(int fd) {
  ::asylo::primitives::MessageWriter input;
  input.Push(fd);
  ::asylo::primitives::MessageReader output;
  const auto status = ::asylo::host_call::NonSystemCallDispatcher(
      ::asylo::host_call::kIsAttyHandler, &input, &output);
  if (!status.ok()) {
    abort();
  }

  int result = output.next<int>();

  // isatty() returns 1 if fd is an open file descriptor referring to a
  // terminal; otherwise 0 is returned, and errno is set to indicate the error.
  if (result == 0) {
    int klinux_errno = output.next<int>();
    errno = FromkLinuxErrorNumber(klinux_errno);
  }
  return result;
}

int enc_untrusted_usleep(useconds_t usec) {
  ::asylo::primitives::MessageWriter input;
  input.Push(usec);
  ::asylo::primitives::MessageReader output;
  asylo::primitives::PrimitiveStatus status =
      asylo::host_call::NonSystemCallDispatcher(
          asylo::host_call::kUSleepHandler, &input, &output);
  if (!status.ok()) {
    abort();
  }

  int result = output.next<int>();

  // usleep() returns 0 on success. On error, -1 is returned, with errno set to
  // indicate the cause of the error.
  if (result == -1) {
    int klinux_errno = output.next<int>();
    errno = FromkLinuxErrorNumber(klinux_errno);
  }

  return result;
}

int enc_untrusted_fstat(int fd, struct stat *statbuf) {
  struct klinux_stat stat_kernel;
  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_fstat, fd, &stat_kernel);

  if (FromkLinuxStat(&stat_kernel, statbuf)) {
    statbuf->st_mode = FromkLinuxFileModeFlag(stat_kernel.klinux_st_mode);
  }
  return result;
}

int enc_untrusted_fstatfs(int fd, struct statfs *statbuf) {
  struct klinux_statfs statfs_kernel;
  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_fstatfs, fd, &statfs_kernel);

  if (FromkLinuxStatFs(&statfs_kernel, statbuf)) {
    statbuf->f_flags = FromkLinuxStatFsFlags(statfs_kernel.klinux_f_flags);
  }
  return result;
}

int enc_untrusted_lstat(const char *pathname, struct stat *statbuf) {
  struct klinux_stat stat_kernel;
  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_lstat, pathname, &stat_kernel);

  if (FromkLinuxStat(&stat_kernel, statbuf)) {
    statbuf->st_mode = FromkLinuxFileModeFlag(stat_kernel.klinux_st_mode);
  }
  return result;
}

int enc_untrusted_stat(const char *pathname, struct stat *statbuf) {
  struct klinux_stat stat_kernel;
  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_stat, pathname, &stat_kernel);
  if (FromkLinuxStat(&stat_kernel, statbuf)) {
    statbuf->st_mode = FromkLinuxFileModeFlag(stat_kernel.klinux_st_mode);
  }
  return result;
}

int enc_untrusted_statfs(const char *pathname, struct statfs *statbuf) {
  struct klinux_statfs statfs_kernel;
  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_statfs, pathname, &statfs_kernel);

  if (FromkLinuxStatFs(&statfs_kernel, statbuf)) {
    statbuf->f_flags = FromkLinuxStatFsFlags(statfs_kernel.klinux_f_flags);
  }
  return result;
}

int64_t enc_untrusted_sysconf(int name) {
  int kLinux_name = TokLinuxSysconfConstant(name);
  if (kLinux_name == -1) {
    errno = EINVAL;
    return -1;
  }

  ::asylo::primitives::MessageWriter input;
  input.Push(kLinux_name);
  ::asylo::primitives::MessageReader output;
  asylo::primitives::PrimitiveStatus status =
      asylo::host_call::NonSystemCallDispatcher(
          asylo::host_call::kSysconfHandler, &input, &output);
  if (!status.ok()) {
    asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_sysconf failed.");
  }

  int64_t result = output.next<int>();
  if (result == -1) {
    int klinux_errno = output.next<int>();
    errno = FromkLinuxErrorNumber(klinux_errno);
  }

  return result;
}

int enc_untrusted_close(int fd) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_close,
                                             fd);
}

void *enc_untrusted_realloc(void *ptr, size_t size) {
  ::asylo::primitives::MessageWriter input;
  input.Push(reinterpret_cast<uint64_t>(ptr));
  input.Push(static_cast<uint64_t>(size));

  ::asylo::primitives::MessageReader output;
  asylo::primitives::PrimitiveStatus status =
      asylo::host_call::NonSystemCallDispatcher(
          asylo::host_call::kReallocHandler, &input, &output);

  if (!status.ok()) {
    asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_realloc failed.");
  }
  void *result = output.next<void *>();

  // realloc only sets the errno (ENOMEM) when output pointer is null and a
  // non-zero |size| is provided.
  if (result == nullptr && size != 0) {
    int klinux_errno = output.next<int>();
    errno = FromkLinuxErrorNumber(klinux_errno);
  }
  return result;
}

uint32_t enc_untrusted_sleep(uint32_t seconds) {
  ::asylo::primitives::MessageWriter input;
  input.Push<uint32_t>(seconds);
  ::asylo::primitives::MessageReader output;
  asylo::primitives::PrimitiveStatus status =
      asylo::host_call::NonSystemCallDispatcher(asylo::host_call::kSleepHandler,
                                                &input, &output);
  if (!status.ok()) {
    asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_sleep failed");
  }

  // Returns sleep's return value directly since it doesn't set errno.
  return output.next<uint32_t>();
}

int enc_untrusted_nanosleep(const struct timespec *req, struct timespec *rem) {
  struct kLinux_timespec klinux_req;
  if (!TokLinuxtimespec(req, &klinux_req)) {
    errno = EINVAL;
    return -1;
  }
  struct kLinux_timespec klinux_rem;

  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_nanosleep, &klinux_req, &klinux_rem);
  FromkLinuxtimespec(&klinux_rem, rem);
  return result;
}

int enc_untrusted_clock_gettime(clockid_t clk_id, struct timespec *tp) {
  clockid_t klinux_clk_id = TokLinuxClockId(clk_id);
  struct kLinux_timespec klinux_tp;
  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_clock_gettime,
      static_cast<int64_t>(klinux_clk_id), &klinux_tp);
  FromkLinuxtimespec(&klinux_tp, tp);
  return result;
}

int enc_untrusted_bind(int sockfd, const struct sockaddr *addr,
                       socklen_t addrlen) {
  if (addr->sa_family == AF_UNSPEC) {
    asylo::primitives::TrustedPrimitives::DebugPuts(
        "AF_UNSPEC provided for sa_family.");
    return 0;
  }

  struct klinux_sockaddr *arg_sockaddr = nullptr;
  socklen_t arg_addrlen = 0;
  struct klinux_sockaddr_un klinux_sock_un;
  struct klinux_sockaddr_in klinux_sock_in;
  struct klinux_sockaddr_in6 klinux_sock_in6;

  if (addr->sa_family == AF_UNIX) {
    if (!SockaddrTokLinuxSockaddrUn(addr, addrlen, &klinux_sock_un)) {
      errno = EINVAL;
      return -1;
    }
    arg_sockaddr = reinterpret_cast<klinux_sockaddr *>(&klinux_sock_un);
    arg_addrlen = sizeof(struct klinux_sockaddr_un);
  } else if (addr->sa_family == AF_INET) {
    if (!SockaddrTokLinuxSockaddrIn(addr, addrlen, &klinux_sock_in)) {
      errno = EINVAL;
      return -1;
    }
    arg_sockaddr = reinterpret_cast<klinux_sockaddr *>(&klinux_sock_in);
    arg_addrlen = sizeof(struct klinux_sockaddr_in);
  } else if (addr->sa_family == AF_INET6) {
    if (!SockaddrTokLinuxSockaddrIn6(addr, addrlen, &klinux_sock_in6)) {
      errno = EINVAL;
      return -1;
    }
    arg_sockaddr = reinterpret_cast<klinux_sockaddr *>(&klinux_sock_in6);
    arg_addrlen = sizeof(struct klinux_sockaddr_in6);
  } else {
    asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "sockaddr family not supported.");
  }

  if (!arg_sockaddr) {
    errno = EINVAL;
    return -1;
  }
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_bind,
                                             sockfd, arg_sockaddr, arg_addrlen);
}

int enc_untrusted_connect(int sockfd, const struct sockaddr *addr,
                          socklen_t addrlen) {
  struct klinux_sockaddr *arg_sockaddr = nullptr;
  socklen_t arg_addrlen = 0;
  struct klinux_sockaddr_un klinux_sock_un;
  struct klinux_sockaddr_in klinux_sock_in;
  struct klinux_sockaddr_in6 klinux_sock_in6;

  if (addr->sa_family == AF_UNIX) {
    if (!SockaddrTokLinuxSockaddrUn(addr, addrlen, &klinux_sock_un)) {
      errno = EINVAL;
      return -1;
    }
    arg_sockaddr = reinterpret_cast<klinux_sockaddr *>(&klinux_sock_un);
    arg_addrlen = sizeof(struct klinux_sockaddr_un);
  } else if (addr->sa_family == AF_INET) {
    if (!SockaddrTokLinuxSockaddrIn(addr, addrlen, &klinux_sock_in)) {
      errno = EINVAL;
      return -1;
    }
    arg_sockaddr = reinterpret_cast<klinux_sockaddr *>(&klinux_sock_in);
    arg_addrlen = sizeof(struct klinux_sockaddr_in);
  } else if (addr->sa_family == AF_INET6) {
    if (!SockaddrTokLinuxSockaddrIn6(addr, addrlen, &klinux_sock_in6)) {
      errno = EINVAL;
      return -1;
    }
    arg_sockaddr = reinterpret_cast<klinux_sockaddr *>(&klinux_sock_in6);
    arg_addrlen = sizeof(struct klinux_sockaddr_in6);
  } else {
    asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "sockaddr family not supported.");
  }
  if (!arg_sockaddr) {
    errno = EINVAL;
    return -1;
  }
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_connect,
                                             sockfd, arg_sockaddr, arg_addrlen);
}

ssize_t enc_untrusted_sendmsg(int sockfd, const struct msghdr *msg, int flags) {
  size_t total_message_size = CalculateTotalSize(msg);
  std::unique_ptr<char[]> msg_iov_buffer(new char[total_message_size]);
  size_t copied_bytes = 0;
  for (int i = 0; i < msg->msg_iovlen; ++i) {
    memcpy(msg_iov_buffer.get() + copied_bytes, msg->msg_iov[i].iov_base,
           msg->msg_iov[i].iov_len);
    copied_bytes += msg->msg_iov[i].iov_len;
  }

  ::asylo::primitives::MessageWriter input;
  input.Push(sockfd);
  input.PushByReference(
      ::asylo::primitives::Extent{msg->msg_name, msg->msg_namelen});
  input.PushByReference(
      ::asylo::primitives::Extent{msg_iov_buffer.get(), total_message_size});
  input.PushByReference(
      ::asylo::primitives::Extent{msg->msg_control, msg->msg_controllen});
  input.Push(msg->msg_flags);
  input.Push(flags);
  ::asylo::primitives::MessageReader output;

  const auto status = ::asylo::host_call::NonSystemCallDispatcher(
      ::asylo::host_call::kSendMsgHandler, &input, &output);
  if (!status.ok()) {
    abort();
  }

  ssize_t result = output.next<ssize_t>();

  // sendmsg() returns the number of characters sent. On error, -1 is returned,
  // with errno set to indicate the cause of the error.
  if (result == -1) {
    int klinux_errno = output.next<int>();
    errno = FromkLinuxErrorNumber(klinux_errno);
  }

  return result;
}

ssize_t enc_untrusted_recvmsg(int sockfd, struct msghdr *msg, int flags) {
  size_t total_buffer_size = CalculateTotalSize(msg);

  ::asylo::primitives::MessageWriter input;
  input.Push(sockfd);
  input.Push<uint64_t>(msg->msg_namelen);
  input.Push<uint64_t>(total_buffer_size);
  input.Push<uint64_t>(msg->msg_controllen);
  input.Push(msg->msg_flags);
  input.Push(flags);

  ::asylo::primitives::MessageReader output;

  const auto status = ::asylo::host_call::NonSystemCallDispatcher(
      ::asylo::host_call::kRecvMsgHandler, &input, &output);

  if (!status.ok()) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_recvmsg host call failed. Aborting");
  }

  ssize_t result = output.next<ssize_t>();
  int klinux_errno = output.next<int>();

  // recvmsg() returns the number of characters received. On error, -1 is
  // returned, with errno set to indicate the cause of the error.
  if (result == -1) {
    errno = FromkLinuxErrorNumber(klinux_errno);
    return result;
  }

  auto msg_name_extent = output.next();
  msg->msg_namelen = msg_name_extent.size();
  memcpy(msg->msg_name, msg_name_extent.As<char>(), msg->msg_namelen);

  // A single buffer is passed from the untrusted side, copy it into the
  // scattered buffers inside the enclave.
  auto msg_iov_extent = output.next();
  size_t total_bytes = msg_iov_extent.size();
  size_t bytes_copied = 0;
  for (int i = 0; i < msg->msg_iovlen && bytes_copied < total_bytes; ++i) {
    size_t bytes_to_copy =
        std::min(msg->msg_iov[i].iov_len, total_bytes - bytes_copied);
    memcpy(msg->msg_iov[i].iov_base, msg_iov_extent.As<char>() + bytes_copied,
           bytes_to_copy);
    bytes_copied += bytes_to_copy;
  }

  auto msg_control_extent = output.next();
  msg->msg_controllen = msg_control_extent.size();
  memcpy(msg->msg_control, msg_control_extent.As<char>(), msg->msg_controllen);

  return result;
}

int enc_untrusted_getsockname(int sockfd, struct sockaddr *addr,
                              socklen_t *addrlen) {
  if (!addr || !addrlen) {
    errno = EFAULT;
    return -1;
  }
  // Guard against -1 being passed as addrlen even though it's unsigned.
  if (*addrlen == 0 || *addrlen > INT32_MAX) {
    errno = EINVAL;
    return -1;
  }

  ::asylo::primitives::MessageWriter input;
  input.Push<int>(sockfd);
  ::asylo::primitives::MessageReader output;
  const auto status = ::asylo::host_call::NonSystemCallDispatcher(
      ::asylo::host_call::kGetSocknameHandler, &input, &output);

  if (!status.ok()) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_getsockname failed. Aborting");
  }
  if (output.size() != 3) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "Expected 3 arguments in output for enc_untrusted_getsockname. "
        "Aborting");
  }

  int result = output.next<int>();
  int klinux_errno = output.next<int>();

  // getsockname() returns 0 on success. On error, -1 is returned, with errno
  // set to indicate the cause of the error.
  if (result == -1) {
    errno = FromkLinuxErrorNumber(klinux_errno);
    return result;
  }

  auto klinux_sockaddr_ext = output.next();
  const struct klinux_sockaddr *klinux_addr =
      klinux_sockaddr_ext.As<struct klinux_sockaddr>();
  FromkLinuxSockAddr(klinux_addr, klinux_sockaddr_ext.size(), addr, addrlen,
                     asylo::primitives::TrustedPrimitives::BestEffortAbort);
  if (!addr) {
    errno = EFAULT;
    return -1;
  }
  return result;
}

int enc_untrusted_accept(int sockfd, struct sockaddr *addr,
                         socklen_t *addrlen) {
  ::asylo::primitives::MessageWriter input;
  input.Push<int>(sockfd);
  ::asylo::primitives::MessageReader output;
  const auto status = ::asylo::host_call::NonSystemCallDispatcher(
      ::asylo::host_call::kAcceptHandler, &input, &output);

  if (!status.ok()) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_accept failed. Aborting");
  }
  if (output.size() != 3) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "Expected 3 arguments in output for enc_untrusted_accept. Aborting");
  }

  int result = output.next<int>();
  int klinux_errno = output.next<int>();

  // accept() returns -1 on failure, with errno set to indicate the cause
  // of the error.
  if (result == -1) {
    errno = FromkLinuxErrorNumber(klinux_errno);
    return result;
  }

  auto klinux_sockaddr_ext = output.next();
  const struct klinux_sockaddr *klinux_addr =
      klinux_sockaddr_ext.As<struct klinux_sockaddr>();
  FromkLinuxSockAddr(klinux_addr, klinux_sockaddr_ext.size(), addr, addrlen,
                     asylo::primitives::TrustedPrimitives::BestEffortAbort);
  return result;
}

int enc_untrusted_getpeername(int sockfd, struct sockaddr *addr,
                              socklen_t *addrlen) {
  if (!addr || !addrlen) {
    errno = EFAULT;
    return -1;
  }
  // Guard against -1 being passed as addrlen even though it's unsigned.
  if (*addrlen == 0 || *addrlen > INT32_MAX) {
    errno = EINVAL;
    return -1;
  }
  ::asylo::primitives::MessageWriter input;
  input.Push<int>(sockfd);
  ::asylo::primitives::MessageReader output;
  const auto status = ::asylo::host_call::NonSystemCallDispatcher(
      ::asylo::host_call::kGetPeernameHandler, &input, &output);

  if (!status.ok()) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_getpeername host call failed. Aborting");
  }
  if (output.size() != 3) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "Expected 3 arguments in output for enc_untrusted_getpeername. "
        "Aborting");
  }

  int result = output.next<int>();
  int klinux_errno = output.next<int>();

  // getpeername() returns -1 on failure, with errno set to indicate the cause
  // of the error.
  if (result == -1) {
    errno = FromkLinuxErrorNumber(klinux_errno);
    return result;
  }

  auto klinux_sockaddr_ext = output.next();
  const struct klinux_sockaddr *klinux_addr =
      klinux_sockaddr_ext.As<struct klinux_sockaddr>();
  FromkLinuxSockAddr(klinux_addr, klinux_sockaddr_ext.size(), addr, addrlen,
                     asylo::primitives::TrustedPrimitives::BestEffortAbort);
  return result;
}

ssize_t enc_untrusted_recvfrom(int sockfd, void *buf, size_t len, int flags,
                               struct sockaddr *src_addr, socklen_t *addrlen) {
  int klinux_flags = TokLinuxRecvSendFlag(flags);
  if (klinux_flags == 0 && flags != 0) {
    errno = EINVAL;
    return -1;
  }

  ::asylo::primitives::MessageWriter input;
  input.Push<int>(sockfd);
  input.Push<uint64_t>(len);
  input.Push<int>(klinux_flags);
  ::asylo::primitives::MessageReader output;
  const auto status = ::asylo::host_call::NonSystemCallDispatcher(
      ::asylo::host_call::kRecvFromHandler, &input, &output);

  if (!status.ok()) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_recvfrom failed. Aborting");
  }
  if (output.size() != 4) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "Expected 4 arguments in output for enc_untrusted_recvfrom. Aborting");
  }

  int result = output.next<int>();
  int klinux_errno = output.next<int>();
  // recvfrom() returns -1 on failure, with errno set to indicate the cause
  // of the error.
  if (result == -1) {
    errno = FromkLinuxErrorNumber(klinux_errno);
    return result;
  }

  auto buffer_received = output.next();
  memcpy(buf, buffer_received.data(), len);

  // If |src_addr| is not NULL, and the underlying protocol provides the source
  // address, this source address is filled in. When |src_addr| is NULL, nothing
  // is filled in; in this case, |addrlen| is not used, and should also be NULL.
  if (src_addr != nullptr && addrlen != nullptr) {
    auto klinux_sockaddr_ext = output.next();
    const struct klinux_sockaddr *klinux_addr =
        klinux_sockaddr_ext.As<struct klinux_sockaddr>();
    FromkLinuxSockAddr(klinux_addr, klinux_sockaddr_ext.size(), src_addr,
                       addrlen,
                       asylo::primitives::TrustedPrimitives::BestEffortAbort);
  }

  return result;
}

int enc_untrusted_select(int nfds, fd_set *readfds, fd_set *writefds,
                         fd_set *exceptfds, struct timeval *timeout) {
  struct klinux_fd_set klinux_readfds, klinux_writefds, klinux_exceptfds;
  struct kLinux_timeval klinux_timeout;

  TokLinuxFdSet(readfds, &klinux_readfds);
  TokLinuxFdSet(writefds, &klinux_writefds);
  TokLinuxFdSet(exceptfds, &klinux_exceptfds);
  TokLinuxtimeval(timeout, &klinux_timeout);

  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_select, nfds, &klinux_readfds, &klinux_writefds,
      &klinux_exceptfds, &klinux_timeout);

  FromkLinuxFdSet(&klinux_readfds, readfds);
  FromkLinuxFdSet(&klinux_writefds, writefds);
  FromkLinuxFdSet(&klinux_exceptfds, exceptfds);
  return result;
}

int enc_untrusted_gettimeofday(struct timeval *tv, struct timezone *tz) {
  struct kLinux_timeval ktv;
  TokLinuxtimeval(tv, &ktv);

  // We do not convert timezone to a klinux value since this struct is expected
  // to be identical across enclave boundary. Besides, the use of the timezone
  // structure is obsolete; the tz argument should normally be specified as
  // NULL.
  int result = EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_gettimeofday, &ktv, tz);
  FromkLinuxtimeval(&ktv, tv);
  return result;
}

int enc_untrusted_fsync(int fd) {
  return EnsureInitializedAndDispatchSyscall(asylo::system_call::kSYS_fsync,
                                             fd);
}

int enc_untrusted_raise(int sig) {
  int klinux_sig = TokLinuxSignalNumber(sig);
  if (klinux_sig < 0) {
    errno = EINVAL;
    return -1;
  }

  ::asylo::primitives::MessageWriter input;
  input.Push<int>(klinux_sig);
  ::asylo::primitives::MessageReader output;
  const auto status = ::asylo::host_call::NonSystemCallDispatcher(
      ::asylo::host_call::kRaiseHandler, &input, &output);

  if (!status.ok()) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "raise host call failed. Aborting");
  }

  int result = output.next<int>();
  int klinux_errno = output.next<int>();
  if (result != 0) {
    errno = FromkLinuxErrorNumber(klinux_errno);
  }
  return result;
}

}  // extern "C"
