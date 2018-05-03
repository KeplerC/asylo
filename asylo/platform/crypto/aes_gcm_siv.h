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

#ifndef ASYLO_PLATFORM_CRYPTO_AES_GCM_SIV_H_
#define ASYLO_PLATFORM_CRYPTO_AES_GCM_SIV_H_

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <memory>
#include <vector>

#include "absl/strings/str_cat.h"
#include "asylo/util/logging.h"
#include "asylo/identity/util/bytes.h"
#include "asylo/platform/crypto/nonce_generator.h"
#include "asylo/platform/crypto/util/bssl_util.h"
#include "asylo/util/cleansing_types.h"
#include "asylo/util/status.h"
#include "asylo/util/statusor.h"

namespace asylo {

constexpr size_t kAesGcmSivNonceSize = 12;

// AesGcmNoncegenerator is a 96-bit NonceGenerator that returns a uniformly
// distributed random nonce on each invocation of NextNonce().
class AesGcmSivNonceGenerator : public NonceGenerator<kAesGcmSivNonceSize> {
 public:
  using AesGcmSivNonce = UnsafeBytes<kAesGcmSivNonceSize>;

  // Implements NextNonce() from NonceGenerator.
  Status NextNonce(const std::vector<uint8_t> &key_id,
                   AesGcmSivNonce *nonce) override;
};

// AesGcmSivCryptor is an AEAD cryptor that provides Seal() and Open()
// functionality using the AES GCM SIV cipher for both 128-bit and 256-bit keys.
// The class must be constructed using a pointer to a 96-bit NonceGenerator. If
// the NonceGenerator is thread-safe, then the constructed object is also
// thread-safe.
//
// The Seal() and Open() methods provided by this cryptor are template methods
// that operate on "byte containers." A byte container is a C++ object that
// meets the following requirements:
//  1. It stores values that are each 1-byte in size.
//  2. It provides size() and resize() methods (although the resize() method
//     could be a fake resize method that does not actually change the size of
//     the container).
//  3. It provides forward and reverse random-access iterators.
//  4. It provides operator[]() and at() accessor methods.
//  5. It defines value_type and allocator_type aliases.
// A byte container of type T is considered to be self-cleansing if
// T::allocator_type is same as CleansingAllocator<typename T::value_type>.
class AesGcmSivCryptor {
 public:
  // Constructs an AES GCM SIV cryptor that enforces the input
  // |message_size_limit| and utilizes |generator| to generate nonces.
  // The constructed object takes ownership of |generator|.
  AesGcmSivCryptor(size_t message_size_limit,
                   NonceGenerator<kAesGcmSivNonceSize> *nonce_generator)
      : message_size_limit_{message_size_limit},
        nonce_generator_{nonce_generator} {}
  ~AesGcmSivCryptor() = default;

  // AEAD Seal. |key|, |additional_data|, |plaintext|, *|nonce|, and
  // *|ciphertext| must be valid byte containers.
  template <typename ContainerT, typename ContainerU, typename ContainerV,
            typename ContainerW, typename ContainerX>
  Status Seal(const ContainerT &key, const ContainerU &additional_data,
              const ContainerV &plaintext, ContainerW *nonce,
              ContainerX *ciphertext) {
    static_assert(
        sizeof(typename ContainerT::value_type) == 1,
        "Template parameter ContainerT is not a valid byte container");
    static_assert(
        sizeof(typename ContainerU::value_type) == 1,
        "Template parameter ContainerU is not a valid byte container");
    static_assert(
        sizeof(typename ContainerV::value_type) == 1,
        "Template parameter ContainerV is not a valid byte container");
    static_assert(
        sizeof(typename ContainerW::value_type) == 1,
        "Template parameter ContainerW is not a valid byte container");
    static_assert(
        sizeof(typename ContainerX::value_type) == 1,
        "Template parameter ContainerX is not a valid byte container");

    // Pick the appropriate EVP_AEAD based on the key length.
    StatusOr<EVP_AEAD const *> aead_result = EvpAead(key.size());
    if (!aead_result.ok()) {
      return aead_result.status();
    }
    EVP_AEAD const *const aead = aead_result.ValueOrDie();

    if (additional_data.size() + plaintext.size() > message_size_limit_) {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "Message size is too large");
    }

    if (nonce_generator_->nonce_size() != EVP_AEAD_nonce_length(aead)) {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "NonceGenerator produces nonces of incorrect length");
    }

    // Get the next nonce from the nonce generator into a local variable, and
    // also copy it to the output. This function maintains its own copy of the
    // nonce so that an entity outside this function would not be able to
    // change the value of the nonce while it is being used.
    UnsafeBytes<kAesGcmSivNonceSize> nonce_copy;
    std::vector<uint8_t> key_id(SHA256_DIGEST_LENGTH);
    if (nonce_generator_->uses_key_id()) {
      SHA256(reinterpret_cast<const uint8_t *>(key.data()), key.size(),
             key_id.data());
    }
    Status status = nonce_generator_->NextNonce(key_id, &nonce_copy);

    if (!status.ok()) {
      return status;
    }
    nonce->resize(nonce_copy.size());

    // Since the Bytes template class provides a fake resize method that does
    // not actually change the size of the container, make sure that *|nonce|
    // actually has the correct size.
    if (nonce->size() != nonce_copy.size()) {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "Could not resize *|nonce| to correct size");
    }
    std::copy(nonce_copy.cbegin(), nonce_copy.cend(), nonce->begin());

    size_t max_ciphertext_length =
        plaintext.size() + EVP_AEAD_max_overhead(aead);

    // Create temporary storage for generating the ciphertext.
    std::vector<uint8_t> tmp_ciphertext(max_ciphertext_length);

    // Initialize the AEAD context.
    EVP_AEAD_CTX context;
    if (EVP_AEAD_CTX_init(
            &context, aead, reinterpret_cast<const uint8_t *>(key.data()),
            key.size(), EVP_AEAD_max_tag_len(aead), nullptr) != 1) {
      return Status(
          error::GoogleError::INTERNAL,
          absl::StrCat("EVP_AEAD_CTX_init failed: ", BsslLastErrorString()));
    }

    // Perform actual encryption.
    size_t ciphertext_length = 0;
    if (EVP_AEAD_CTX_seal(
            &context, tmp_ciphertext.data(), &ciphertext_length,
            tmp_ciphertext.size(), nonce_copy.data(), nonce_copy.size(),
            reinterpret_cast<const uint8_t *>(plaintext.data()),
            plaintext.size(),
            reinterpret_cast<const uint8_t *>(additional_data.data()),
            additional_data.size()) != 1) {
      EVP_AEAD_CTX_cleanup(&context);
      return Status(
          error::GoogleError::INTERNAL,
          absl::StrCat("EVP_AEAD_CTX_seal failed: ", BsslLastErrorString()));
    }

    // Resize and copy the output.
    tmp_ciphertext.resize(ciphertext_length);
    ciphertext->resize(ciphertext_length);

    // Since the Bytes template class provides a fake resize method that does
    // not actually change the size of the container, make sure that
    // *|ciphertext| actually has the correct size.
    if (ciphertext->size() != ciphertext_length) {
      EVP_AEAD_CTX_cleanup(&context);
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "Could not resize *|ciphertext| to correct size");
    }
    std::copy(tmp_ciphertext.cbegin(), tmp_ciphertext.cend(),
              ciphertext->begin());

    EVP_AEAD_CTX_cleanup(&context);
    return Status::OkStatus();
  }

  // AEAD Open. |key|, |additional_data|, |ciphertext|, |nonce|, and
  // *|plaintext| must be valid byte containers. Additionally, *|plaintext|
  // must be a self-cleansing byte container.
  template <typename ContainerT, typename ContainerU, typename ContainerV,
            typename ContainerW, typename ContainerX>
  Status Open(const ContainerT &key, const ContainerU &additional_data,
              const ContainerV &ciphertext, const ContainerW &nonce,
              ContainerX *plaintext) {
    static_assert(
        sizeof(typename ContainerT::value_type) == 1,
        "Template parameter ContainerT is not a valid byte container");
    static_assert(
        sizeof(typename ContainerU::value_type) == 1,
        "Template parameter ContainerU is not a valid byte container");
    static_assert(
        sizeof(typename ContainerV::value_type) == 1,
        "Template parameter ContainerV is not a valid byte container");
    static_assert(
        sizeof(typename ContainerW::value_type) == 1,
        "Template parameter ContainerW is not a valid byte container");
    static_assert(
        sizeof(typename ContainerX::value_type) == 1,
        "Template parameter ContainerX is not a valid byte container");
    using PlaintextContainerT =
        typename std::remove_reference<decltype(*plaintext)>::type;
    using PlaintextValueT = typename PlaintextContainerT::value_type;
    static_assert(std::is_same<typename PlaintextContainerT::allocator_type,
                               CleansingAllocator<PlaintextValueT>>::value,
                  "Ciphertext container must be self-cleansing");

    // Pick the appropriate EVP_AEAD based on the key length.
    StatusOr<EVP_AEAD const *> aead_result = EvpAead(key.size());
    if (!aead_result.ok()) {
      return aead_result.status();
    }
    EVP_AEAD const *const aead = aead_result.ValueOrDie();

    if (nonce.size() != EVP_AEAD_nonce_length(aead)) {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "|nonce| has incorrect length");
    }

    // Copy the supplied nonce into a local variable. This function maintains
    // its own copy of the nonce so that an entity outside this function would
    // not be able to change the value of the nonce while it is being used.
    std::vector<uint8_t> nonce_copy;
    nonce_copy.resize(nonce.size());
    std::copy(nonce.cbegin(), nonce.cend(), nonce_copy.begin());

    // Allocate temporary storage for the plaintext. Since the plaintext is
    // sensitive, the temporary storage consists of a self-cleansing vector to
    // facilitate RAII-style cleansing when exiting the function.
    CleansingVector<uint8_t> tmp_plaintext;
    tmp_plaintext.resize(ciphertext.size());

    // Initialize the AEAD context.
    EVP_AEAD_CTX context;
    if (EVP_AEAD_CTX_init(
            &context, aead, reinterpret_cast<const uint8_t *>(key.data()),
            key.size(), EVP_AEAD_max_tag_len(aead), nullptr) != 1) {
      return Status(
          error::GoogleError::INTERNAL,
          absl::StrCat("EVP_AEAD_CTX_init failed: ", BsslLastErrorString()));
    }

    // Perform the actual decryption.
    size_t plaintext_length = 0;
    if (EVP_AEAD_CTX_open(
            &context, tmp_plaintext.data(), &plaintext_length,
            tmp_plaintext.size(),
            reinterpret_cast<const uint8_t *>(nonce.data()), nonce.size(),
            reinterpret_cast<const uint8_t *>(ciphertext.data()),
            ciphertext.size(),
            reinterpret_cast<const uint8_t *>(additional_data.data()),
            additional_data.size()) != 1) {
      EVP_AEAD_CTX_cleanup(&context);
      return Status(
          error::GoogleError::INTERNAL,
          absl::StrCat("EVP_AEAD_CTX_open failed: ", BsslLastErrorString()));
    }

    // Resize and copy the output.
    tmp_plaintext.resize(plaintext_length);
    plaintext->resize(plaintext_length);

    // Since the Bytes template class provides a fake resize method that does
    // not actually change the size of the container, make sure that
    // *|plaintext| actually has the correct size.
    if (plaintext->size() != plaintext_length) {
      EVP_AEAD_CTX_cleanup(&context);
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "Could not resize *|plaintext| to correct size");
    }
    std::copy(tmp_plaintext.cbegin(), tmp_plaintext.cend(), plaintext->begin());

    EVP_AEAD_CTX_cleanup(&context);
    return Status::OkStatus();
  }

 private:
  StatusOr<EVP_AEAD const *> EvpAead(size_t key_size) {
    // Pick the appropriate EVP_AEAD based on the key length.
    EVP_AEAD const *const aead_128 = EVP_aead_aes_128_gcm_siv();
    EVP_AEAD const *const aead_256 = EVP_aead_aes_256_gcm_siv();

    if (key_size == EVP_AEAD_key_length(aead_128)) {
      return aead_128;
    } else if (key_size == EVP_AEAD_key_length(aead_256)) {
      return aead_256;
    } else {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    absl::StrCat("Key size ", key_size, " is invalid"));
    }
  }

  const size_t message_size_limit_;
  std::unique_ptr<NonceGenerator<kAesGcmSivNonceSize>> nonce_generator_;
};

}  // namespace asylo

#endif  // ASYLO_PLATFORM_CRYPTO_AES_GCM_SIV_H_
