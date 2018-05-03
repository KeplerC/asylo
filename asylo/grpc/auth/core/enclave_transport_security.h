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

#ifndef ASYLO_GRPC_AUTH_CORE_ENCLAVE_TRANSPORT_SECURITY_H_
#define ASYLO_GRPC_AUTH_CORE_ENCLAVE_TRANSPORT_SECURITY_H_

#include "asylo/grpc/auth/core/assertion_description.h"
#include "asylo/grpc/auth/util/safe_string.h"
#include "src/core/tsi/transport_security_interface.h"

// Value for TSI_CERTIFICATE_TYPE_PEER_PROPERTY property for enclave
// certificates.
#define TSI_ENCLAVE_CERTIFICATE_TYPE "enclave_security"

// Peer properties in enclave peer objects.
#define TSI_ENCLAVE_IDENTITIES_PROTO_PEER_PROPERTY \
  "enclave_security_identity_proto"
#define TSI_ENCLAVE_RECORD_PROTOCOL_PEER_PROPERTY \
  "enclave_security_record_protocol"

// Creates an enclave handshaker and places the result in |handshaker|.
// Configures the handshaker with the following options:
//   * |is_client| indicates whether to create a client or server handshaker
//   * |self_assertions| specifies the assertions that the handshaker is willing
//   to present during the handshake
//   * |accepted_peer_assertions| specifies the assertions that the handshaker
//   is willing to accept from the peer during the handshake
//   * |additional_authenticated_data| is data to be authenticated as part of
//   the handshake
tsi_result tsi_enclave_handshaker_create(
    int is_client, const assertion_description_array *self_assertions,
    const assertion_description_array *accepted_peer_assertions,
    const safe_string *additional_authenticated_data,
    tsi_handshaker **handshaker);

#endif  // ASYLO_GRPC_AUTH_CORE_ENCLAVE_TRANSPORT_SECURITY_H_
