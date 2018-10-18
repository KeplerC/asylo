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

#include <gtest/gtest.h>
#include "asylo/client.h"
#include "asylo/enclave.pb.h"
#include "asylo/enclave_manager.h"
#include "gflags/gflags.h"
#include "asylo/test/util/status_matchers.h"

DEFINE_string(enclave_section, "", "The ELF section the enclave is located in");

namespace asylo {
namespace {

constexpr char kEnclaveName[] = "enclave";

TEST(EmbeddedEnclaveTest, EnclaveLoadsAndRuns) {
  // Retrieve the EnclaveManager.
  EnclaveManager::Configure(EnclaveManagerOptions());
  auto manager_result = EnclaveManager::Instance();
  ASSERT_THAT(manager_result, IsOk());
  EnclaveManager *manager = manager_result.ValueOrDie();

  // Load the enclave.
  SgxEmbeddedLoader loader(FLAGS_enclave_section, /*debug=*/true);
  EnclaveConfig config;
  ASSERT_THAT(manager->LoadEnclave(kEnclaveName, loader, config), IsOk());
  EnclaveClient *client = manager->GetClient(kEnclaveName);

  // Enter the enclave with a no-op.
  EnclaveInput input;
  EnclaveOutput output;
  EXPECT_THAT(client->EnterAndRun(input, &output), IsOk());

  // Destroy the enclave.
  EnclaveFinal final_input;
  EXPECT_THAT(manager->DestroyEnclave(client, final_input), IsOk());
}

}  // namespace
}  // namespace asylo
