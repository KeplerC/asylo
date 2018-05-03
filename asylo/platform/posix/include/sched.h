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

#ifndef ASYLO_PLATFORM_POSIX_INCLUDE_SCHED_H_
#define ASYLO_PLATFORM_POSIX_INCLUDE_SCHED_H_

#include <internal/sched.h>

#ifdef __cplusplus
extern "C" {
#endif

#include_next <sched.h>

// The maximum number of CPUs we support. Should match BRIDGE_CPU_SET_MAX_CPUS
// in third_party/asylo/platform/common/bridge_types.h.
#define CPU_SETSIZE CPU_SET_MAX_CPUS

// Represents a set of (up to) CPU_SETSIZE CPUs as a bitset.
typedef CpuSet cpu_set_t;

// We implement a subset of the macros from CPU_SET(3) here.

#define CPU_ZERO(set) CpuSetZero((set))

#define CPU_SET(cpu, set) CpuSetAddBit((cpu), (set))

#define CPU_CLR(cpu, set) CpuSetClearBit((cpu), (set))

#define CPU_ISSET(cpu, set) CpuSetCheckBit((cpu), (set))

#define CPU_COUNT(set) CpuSetCountBits((set))

#define CPU_EQUAL(set1, set2) CpuSetEqual((set1), (set2))

// Calls sched_getaffinity() on the host, then translates the host's mask to a
// bridge_cpu_set_t for transmission across the enclave boundary, and finally
// translates that to the enclave's cpu_set_t type (defined above).
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);

// Implemented as call to host sched_yield().
int sched_yield(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ASYLO_PLATFORM_POSIX_INCLUDE_SCHED_H_
