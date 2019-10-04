/*
 *
 * Copyright 2018 Asylo authors
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

// Stubs invoked by edger8r generated bridge code for ocalls.

// For |domainname| field in struct utsname and pipe2().
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <poll.h>
#include <pwd.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <utime.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iterator>
#include <vector>

#include "absl/memory/memory.h"
#include "asylo/enclave.pb.h"
#include "asylo/util/logging.h"
#include "asylo/platform/arch/sgx/untrusted/generated_bridge_u.h"
#include "asylo/platform/arch/sgx/untrusted/sgx_client.h"
#include "asylo/platform/common/bridge_functions.h"
#include "asylo/platform/common/bridge_proto_serializer.h"
#include "asylo/platform/common/bridge_types.h"
#include "asylo/platform/common/debug_strings.h"
#include "asylo/platform/common/memory.h"
#include "asylo/platform/core/enclave_manager.h"
#include "asylo/platform/core/generic_enclave_client.h"
#include "asylo/platform/core/shared_name.h"
#include "asylo/platform/primitives/sgx/loader.pb.h"
#include "asylo/platform/primitives/sgx/sgx_params.h"
#include "asylo/platform/primitives/sgx/signal_dispatcher.h"
#include "asylo/platform/primitives/sgx/untrusted_sgx.h"
#include "asylo/platform/primitives/util/message.h"
#include "asylo/platform/storage/utils/fd_closer.h"
#include "asylo/util/posix_error_space.h"
#include "asylo/util/status.h"
#include "asylo/util/status_macros.h"

namespace {

// Stores a pointer to a function inside the enclave that translates
// |bridge_signum| to a value inside the enclave and calls the registered signal
// handler for that signal.
static void (*handle_signal_inside_enclave)(int, bridge_siginfo_t *,
                                            void *) = nullptr;

// Translates host |signum| to |bridge_signum|, and calls the function
// registered as the signal handler inside the enclave.
void TranslateToBridgeAndHandleSignal(int signum, siginfo_t *info,
                                      void *ucontext) {
  int bridge_signum = asylo::ToBridgeSignal(signum);
  if (bridge_signum < 0) {
    // Invalid incoming signal number.
    return;
  }
  struct bridge_siginfo_t bridge_siginfo;
  asylo::ToBridgeSigInfo(info, &bridge_siginfo);
  if (handle_signal_inside_enclave) {
    handle_signal_inside_enclave(bridge_signum, &bridge_siginfo, ucontext);
  }
}

// Triggers an ecall to enter an enclave to handle the incoming signal.
//
// In hardware mode, this is registered as the signal handler.
// In simulation mode, this is called if the signal arrives when the TCS is
// inactive.
void EnterEnclaveAndHandleSignal(int signum, siginfo_t *info, void *ucontext) {
  asylo::primitives::EnclaveSignalDispatcher::GetInstance()
      ->EnterEnclaveAndHandleSignal(signum, info, ucontext);
}

// Checks the enclave TCS state to determine which function to call to handle
// the signal. If the TCS is active, calls the signal handler registered inside
// the enclave directly. If the TCS is inactive, triggers an ecall to enter
// enclave and handle the signal.
//
// In simulation mode, this is registered as the signal handler.
void HandleSignalInSim(int signum, siginfo_t *info, void *ucontext) {
  auto client_result = asylo::primitives::EnclaveSignalDispatcher::GetInstance()
                           ->GetClientForSignal(signum);
  if (!client_result.ok()) {
    return;
  }
  asylo::primitives::SgxEnclaveClient *client =
      dynamic_cast<asylo::primitives::SgxEnclaveClient *>(
          client_result.ValueOrDie());
  if (client->IsTcsActive()) {
    TranslateToBridgeAndHandleSignal(signum, info, ucontext);
  } else {
    EnterEnclaveAndHandleSignal(signum, info, ucontext);
  }
}

// Perform a snapshot key transfer from the parent to the child.
asylo::Status DoSnapshotKeyTransfer(asylo::EnclaveManager *manager,
                                    asylo::EnclaveClient *client,
                                    int self_socket, int peer_socket,
                                    bool is_parent) {
  asylo::platform::storage::FdCloser self_socket_closer(self_socket);
  // Close the socket for the other side, and enters the enclave to send the
  // snapshot key through the socket.
  if (close(peer_socket) < 0) {
    return asylo::Status(static_cast<asylo::error::PosixError>(errno),
                         absl::StrCat("close failed: ", strerror(errno)));
  }

  asylo::ForkHandshakeConfig fork_handshake_config;
  fork_handshake_config.set_is_parent(is_parent);
  fork_handshake_config.set_socket(self_socket);
  asylo::GenericEnclaveClient *generic_client =
      dynamic_cast<asylo::GenericEnclaveClient *>(client);
  std::shared_ptr<asylo::primitives::SgxEnclaveClient> primitive_client =
      std::static_pointer_cast<asylo::primitives::SgxEnclaveClient>(
          generic_client->GetPrimitiveClient());
  ASYLO_RETURN_IF_ERROR(primitive_client->EnterAndTransferSecureSnapshotKey(
      fork_handshake_config));

  return asylo::Status::OkStatus();
}

// A helper class to free the snapshot memory allocated during fork.
class SnapshotDataDeleter {
 public:
  explicit SnapshotDataDeleter(const asylo::SnapshotLayoutEntry &entry)
      : ciphertext_deleter_(reinterpret_cast<void *>(entry.ciphertext_base())),
        nonce_deleter_(reinterpret_cast<void *>(entry.nonce_base())) {}

 private:
  asylo::MallocUniquePtr<void> ciphertext_deleter_;
  asylo::MallocUniquePtr<void> nonce_deleter_;
};

}  // namespace

//////////////////////////////////////
//              IO                  //
//////////////////////////////////////

int ocall_untrusted_debug_puts(const char *str) {
  int rc = puts(str);
  // This routine is intended for debugging, so flush immediately to ensure
  // output is written in the event the enclave aborts with buffered output.
  fflush(stdout);
  return rc;
}

void *ocall_untrusted_local_alloc(uint64_t size) {
  void *ret = malloc(static_cast<size_t>(size));
  return ret;
}

void **ocall_enc_untrusted_allocate_buffers(bridge_size_t count,
                                            bridge_size_t size) {
  void **buffers = reinterpret_cast<void **>(
      malloc(static_cast<size_t>(count) * sizeof(void *)));
  for (int i = 0; i < count; i++) {
    buffers[i] = malloc(size);
  }
  return buffers;
}

void ocall_enc_untrusted_deallocate_free_list(void **free_list,
                                              bridge_size_t count) {
  // This function only releases memory on the untrusted heap pointed to by
  // buffer pointers stored in |free_list|, not freeing the |free_list| object
  // itself. The client making the host call is responsible for the deallocation
  // of the |free list| object.
  for (int i = 0; i < count; i++) {
    free(free_list[i]);
  }
}

//////////////////////////////////////
//           inotify.h              //
//////////////////////////////////////

int ocall_enc_untrusted_inotify_read(int fd, bridge_size_t count,
                                     char **serialized_events,
                                     bridge_size_t *serialized_events_len) {
  size_t buf_size =
      std::max(sizeof(struct inotify_event) + NAME_MAX + 1, count);
  char *buf = static_cast<char *>(malloc(buf_size));
  asylo::MallocUniquePtr<char> buf_ptr(buf);
  int bytes_read = read(fd, buf, buf_size);
  if (bytes_read < 0) {
    // Errno will be set by read.
    return -1;
  }
  size_t len = 0;
  if (!asylo::SerializeInotifyEvents(buf, bytes_read, serialized_events,
                                     &len)) {
    return -1;
  }
  *serialized_events_len = static_cast<bridge_size_t>(len);
  return 0;
}

//////////////////////////////////////
//            pwd.h                 //
//////////////////////////////////////

int ocall_enc_untrusted_getpwuid(uid_t uid,
                                 struct BridgePassWd *bridge_password) {
  struct passwd *password = getpwuid(uid);
  if (!password) {
    return -1;
  }
  if (!asylo::ToBridgePassWd(password, bridge_password)) {
    errno = EFAULT;
    return -1;
  }
  return 0;
}

//////////////////////////////////////
//          signal.h                //
//////////////////////////////////////

int ocall_enc_untrusted_register_signal_handler(
    int bridge_signum, const struct BridgeSignalHandler *handler,
    const char *name) {
  std::string enclave_name(name);
  int signum = asylo::FromBridgeSignal(bridge_signum);
  if (signum < 0) {
    errno = EINVAL;
    return -1;
  }
  auto manager_result = asylo::EnclaveManager::Instance();
  if (!manager_result.ok()) {
    return -1;
  }
  // Registers the signal with an enclave so when the signal arrives,
  // EnclaveManager knows which enclave to enter to handle the signal.
  asylo::EnclaveManager *manager = manager_result.ValueOrDie();
  asylo::EnclaveClient *client = manager->GetClient(enclave_name);
  asylo::GenericEnclaveClient *generic_client =
      dynamic_cast<asylo::GenericEnclaveClient *>(client);
  std::shared_ptr<asylo::primitives::SgxEnclaveClient> primitive_client =
      std::static_pointer_cast<asylo::primitives::SgxEnclaveClient>(
          generic_client->GetPrimitiveClient());
  const asylo::primitives::SgxEnclaveClient *old_client =
      asylo::primitives::EnclaveSignalDispatcher::GetInstance()->RegisterSignal(
          signum, primitive_client.get());
  if (old_client) {
    LOG(WARNING) << "Overwriting the signal handler for signal: " << signum
                 << " registered by another enclave";
  }
  struct sigaction newact;
  if (!handler || !handler->sigaction) {
    // Hardware mode: The registered signal handler triggers an ecall to enter
    // the enclave and handle the signal.
    newact.sa_sigaction = &EnterEnclaveAndHandleSignal;
  } else {
    // Simulation mode: The registered signal handler does the same as hardware
    // mode if the TCS is active, or calls the signal handler registered inside
    // the enclave directly if the TCS is inactive.
    handle_signal_inside_enclave = handler->sigaction;
    newact.sa_sigaction = &HandleSignalInSim;
  }
  if (handler) {
    asylo::FromBridgeSigSet(&handler->mask, &newact.sa_mask);
  }
  // Set the flag so that sa_sigaction is registered as the signal handler
  // instead of sa_handler.
  newact.sa_flags = asylo::FromBridgeSignalFlags(handler->flags);
  newact.sa_flags |= SA_SIGINFO;
  struct sigaction oldact;
  return sigaction(signum, &newact, &oldact);
}

//////////////////////////////////////
//          sys/syslog.h            //
//////////////////////////////////////

void ocall_enc_untrusted_openlog(const char *ident, int option, int facility) {
  openlog(ident, asylo::FromBridgeSysLogOption(option),
          asylo::FromBridgeSysLogFacility(facility));
}

void ocall_enc_untrusted_syslog(int priority, const char *message) {
  syslog(asylo::FromBridgeSysLogPriority(priority), "%s", message);
}

//////////////////////////////////////
//         sys/utsname.h            //
//////////////////////////////////////

int ocall_enc_untrusted_uname(struct BridgeUtsName *bridge_utsname_val) {
  if (!bridge_utsname_val) {
    errno = EFAULT;
    return -1;
  }

  struct utsname utsname_val;
  int ret = uname(&utsname_val);
  if (ret != 0) {
    return ret;
  }

  if (!asylo::ConvertUtsName(utsname_val, bridge_utsname_val)) {
    errno = EINTR;
    return -1;
  }

  return ret;
}

//////////////////////////////////////
//            unistd.h              //
//////////////////////////////////////

void ocall_enc_untrusted__exit(int rc) { _exit(rc); }

pid_t ocall_enc_untrusted_fork(const char *enclave_name,
                               bool restore_snapshot) {
  auto manager_result = asylo::EnclaveManager::Instance();
  if (!manager_result.ok()) {
    return -1;
  }
  asylo::EnclaveManager *manager = manager_result.ValueOrDie();
  asylo::GenericEnclaveClient *client =
      dynamic_cast<asylo::GenericEnclaveClient *>(
          manager->GetClient(enclave_name));
  std::shared_ptr<asylo::primitives::SgxEnclaveClient> primitive_client =
      std::static_pointer_cast<asylo::primitives::SgxEnclaveClient>(
          client->GetPrimitiveClient());

  if (!restore_snapshot) {
    // No need to take and restore a snapshot, just set indication that the new
    // enclave is created from fork.
    pid_t pid = fork();
    if (pid == 0) {
      // Set the process ID so that the new enclave created from fork does not
      // reject entry.
      primitive_client->SetProcessId();
    }
    return pid;
  }

  // A snapshot should be taken and restored for fork, take a snapshot of the
  // current enclave memory.
  void *enclave_base_address = primitive_client->GetBaseAddress();
  asylo::SnapshotLayout snapshot_layout;
  asylo::Status status =
      primitive_client->EnterAndTakeSnapshot(&snapshot_layout);
  if (!status.ok()) {
    LOG(ERROR) << "EnterAndTakeSnapshot failed: " << status;
    errno = ENOMEM;
    return -1;
  }

  // The snapshot memory should be freed in both the parent and the child
  // process.
  std::vector<SnapshotDataDeleter> data_deleter_;
  std::vector<SnapshotDataDeleter> bss_deleter_;
  std::vector<SnapshotDataDeleter> heap_deleter_;
  std::vector<SnapshotDataDeleter> thread_deleter_;
  std::vector<SnapshotDataDeleter> stack_deleter_;

  std::transform(snapshot_layout.data().cbegin(), snapshot_layout.data().cend(),
                 std::back_inserter(data_deleter_),
                 [](const asylo::SnapshotLayoutEntry &entry) {
                   return SnapshotDataDeleter(entry);
                 });

  std::transform(snapshot_layout.bss().cbegin(), snapshot_layout.bss().cend(),
                 std::back_inserter(bss_deleter_),
                 [](const asylo::SnapshotLayoutEntry &entry) {
                   return SnapshotDataDeleter(entry);
                 });

  std::transform(snapshot_layout.heap().cbegin(), snapshot_layout.heap().cend(),
                 std::back_inserter(heap_deleter_),
                 [](const asylo::SnapshotLayoutEntry &entry) {
                   return SnapshotDataDeleter(entry);
                 });

  std::transform(snapshot_layout.thread().cbegin(),
                 snapshot_layout.thread().cend(),
                 std::back_inserter(thread_deleter_),
                 [](const asylo::SnapshotLayoutEntry &entry) {
                   return SnapshotDataDeleter(entry);
                 });

  std::transform(snapshot_layout.stack().cbegin(),
                 snapshot_layout.stack().cend(),
                 std::back_inserter(stack_deleter_),
                 [](const asylo::SnapshotLayoutEntry &entry) {
                   return SnapshotDataDeleter(entry);
                 });

  asylo::EnclaveLoadConfig load_config =
      manager->GetLoadConfigFromClient(client);

  // The child enclave should use the same loader as the parent. It loads by an
  // SGX loader or SGX embedded loader depending on the parent enclave.
  if (!load_config.HasExtension(asylo::sgx_load_config)) {
    LOG(ERROR) << "Failed to get the loader for the enclave to fork";
    errno = EFAULT;
    return -1;
  }

  // Create a socket pair used for communication between the parent and child
  // enclave. |socket_pair[0]| is used by the parent enclave and
  // |socket_pair[1]| is used by the child enclave.
  int socket_pair[2];
  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, socket_pair) < 0) {
    LOG(ERROR) << "Failed to create socket pair";
    errno = EFAULT;
    return -1;
  }

  // Create a pipe used to pass the child process fork state to the parent
  // process. If the child process failed to restore the enclave, the parent
  // fork should return error as well.
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    LOG(ERROR) << "Failed to create pipe";
    errno = EFAULT;
    return -1;
  }

  pid_t pid = fork();
  if (pid == -1) {
    return pid;
  }

  size_t enclave_size = primitive_client->GetEnclaveSize();

  if (pid == 0) {
    if (close(pipefd[0]) < 0) {
      LOG(ERROR) << "failed to close pipefd: " << strerror(errno);
      errno = EFAULT;
      return -1;
    }
    // Load an enclave at the same virtual space as the parent.
    load_config.set_name(enclave_name);
    asylo::SgxLoadConfig sgx_config =
        load_config.GetExtension(asylo::sgx_load_config);
    asylo::SgxLoadConfig::ForkConfig fork_config;
    fork_config.set_base_address(
        reinterpret_cast<uint64_t>(enclave_base_address));
    fork_config.set_enclave_size(enclave_size);
    *sgx_config.mutable_fork_config() = fork_config;
    *load_config.MutableExtension(asylo::sgx_load_config) = sgx_config;
    status = manager->LoadEnclave(load_config);
    if (!status.ok()) {
      LOG(ERROR) << "Load new enclave failed:" << status;
      errno = ENOMEM;
      return -1;
    }

    // Verifies that the new enclave is loaded at the same virtual address space
    // as the parent enclave.
    client = dynamic_cast<asylo::GenericEnclaveClient *>(
        manager->GetClient(enclave_name));
    std::shared_ptr<asylo::primitives::SgxEnclaveClient> primitive_client =
        std::static_pointer_cast<asylo::primitives::SgxEnclaveClient>(
            client->GetPrimitiveClient());

    void *child_enclave_base_address = primitive_client->GetBaseAddress();
    if (child_enclave_base_address != enclave_base_address) {
      LOG(ERROR) << "New enclave address: " << child_enclave_base_address
                 << " is different from the parent enclave address: "
                 << enclave_base_address;
      errno = EAGAIN;
      return -1;
    }

    std::string child_result = "Child fork succeeded";
    asylo::Status status = DoSnapshotKeyTransfer(
        manager, client, socket_pair[0], socket_pair[1], /*is_parent=*/false);
    if (!status.ok()) {
      // Inform the parent process about the failure.
      child_result = "Child DoSnapshotKeyTransfer failed";
      if (write(pipefd[1], child_result.data(), child_result.size()) < 0) {
        LOG(ERROR) << "Failed to write child fork result to: " << pipefd[1]
                   << ", error: " << strerror(errno);
        return -1;
      }
      LOG(ERROR) << "DoSnapshotKeyTransfer failed: " << status;
      errno = EFAULT;
      return -1;
    }

    // Enters the child enclave and restore the enclave memory.
    status = primitive_client->EnterAndRestore(snapshot_layout);
    if (!status.ok()) {
      // Inform the parent process about the failure.
      child_result = "Child EnterAndRestore failed";
      if (write(pipefd[1], child_result.data(), child_result.size()) < 0) {
        LOG(ERROR) << "Failed to write child fork result to: " << pipefd[1]
                   << ", error: " << strerror(errno);
        return -1;
      }
      LOG(ERROR) << "EnterAndRestore failed: " << status;
      errno = EAGAIN;
      return -1;
    }
    // Inform the parent that child fork has succeeded.
    if (write(pipefd[1], child_result.data(), child_result.size()) < 0) {
      LOG(ERROR) << "Failed to write child fork result to: " << pipefd[1]
                 << ", error: " << strerror(errno);
      return -1;
    }
  } else {
    if (close(pipefd[1]) < 0) {
      LOG(ERROR) << "Failed to close pipefd: " << strerror(errno);
      errno = EFAULT;
      return -1;
    }
    asylo::Status status = DoSnapshotKeyTransfer(
        manager, client, /*self_socket=*/socket_pair[1],
        /*peer_socket=*/socket_pair[0], /*is_parent=*/true);
    if (!status.ok()) {
      LOG(ERROR) << "DoSnapshotKeyTransfer failed: " << status;
      errno = EFAULT;
      return -1;
    }
    // Wait for the information from the child process to confirm whether the
    // child enclave has been successfully restored. Timeout at 5 seconds.
    const int timeout_seconds = 5;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipefd[0], &read_fds);
    struct timeval timeout;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    int wait_result =
        select(/*nfds=*/pipefd[0] + 1, &read_fds, /*writefds=*/nullptr,
               /*exceptfds=*/nullptr, &timeout);
    if (wait_result < 0) {
      LOG(ERROR) << "Error while waiting for child fork result: "
                 << strerror(errno);
      return -1;
    } else if (wait_result == 0) {
      LOG(ERROR) << "Timeout waiting for fork result from the child";
      errno = EFAULT;
      return -1;
    }
    // Child fork result is ready to be read.
    char buf[64];
    int rc = read(pipefd[0], buf, sizeof(buf));
    if (rc <= 0) {
      LOG(ERROR) << "Failed to read child fork result";
      return -1;
    }
    buf[rc] = '\0';
    if (strncmp(buf, "Child fork succeeded", sizeof(buf)) != 0) {
      LOG(ERROR) << buf;
      return -1;
    }
  }
  return pid;
}

//////////////////////////////////////
//             wait.h               //
//////////////////////////////////////

pid_t ocall_enc_untrusted_wait3(struct BridgeWStatus *bridge_wstatus,
                                int options,
                                struct BridgeRUsage *bridge_usage) {
  struct rusage usage;
  int wstatus;
  pid_t ret = wait3(&wstatus, asylo::FromBridgeWaitOptions(options), &usage);
  asylo::ToBridgeRUsage(&usage, bridge_usage);
  if (bridge_wstatus) {
    *bridge_wstatus = asylo::ToBridgeWStatus(wstatus);
  }
  return ret;
}

pid_t ocall_enc_untrusted_waitpid(pid_t pid,
                                  struct BridgeWStatus *bridge_wstatus,
                                  int options) {
  int wstatus;
  pid_t ret = waitpid(pid, &wstatus, asylo::FromBridgeWaitOptions(options));
  if (bridge_wstatus) {
    *bridge_wstatus = asylo::ToBridgeWStatus(wstatus);
  }
  return ret;
}

//////////////////////////////////////
//           Debugging              //
//////////////////////////////////////

void ocall_enc_untrusted_hex_dump(const void *buf, bridge_size_t nbytes) {
  fprintf(stderr, "%s\n", asylo::buffer_to_hex_string(buf, nbytes).c_str());
}

int ocall_dispatch_untrusted_call(uint64_t selector, void *buffer) {
  asylo::SgxParams *const sgx_params =
      reinterpret_cast<asylo::SgxParams *>(buffer);
  ::asylo::primitives::MessageReader in;
  if (sgx_params->input) {
    in.Deserialize(sgx_params->input, sgx_params->input_size);
    free(const_cast<void *>(sgx_params->input));
  }
  sgx_params->output_size = 0;
  sgx_params->output = nullptr;
  ::asylo::primitives::MessageWriter out;
  const auto status =
      ::asylo::primitives::Client::ExitCallback(selector, &in, &out);
  if (status.ok()) {
    sgx_params->output_size = out.MessageSize();
    if (sgx_params->output_size > 0) {
      sgx_params->output = malloc(sgx_params->output_size);
      out.Serialize(sgx_params->output);
    }
  }
  return status.error_code();
}

void ocall_untrusted_local_free(void *buffer) { free(buffer); }
