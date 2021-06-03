/*
 * Copyright 2020 Asylo authors
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
 */

#ifndef ASYLO_CRYPTO_ECDSA_SIGNING_KEY_TEST_H_
#define ASYLO_CRYPTO_ECDSA_SIGNING_KEY_TEST_H_

#include <openssl/base.h>
#include <openssl/ec.h>
#include <openssl/nid.h>
#include <openssl/rand.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "asylo/crypto/algorithms.pb.h"
#include "asylo/crypto/ecdsa_signing_key.h"
#include "asylo/crypto/fake_signing_key.h"
#include "asylo/crypto/keys.pb.h"
#include "asylo/crypto/signing_key.h"
#include "asylo/crypto/util/byte_container_util.h"
#include "asylo/crypto/util/byte_container_view.h"
#include "asylo/util/logging.h"
#include "asylo/test/util/proto_matchers.h"
#include "asylo/test/util/status_matchers.h"
#include "asylo/test/util/string_matchers.h"
#include "asylo/util/cleansing_types.h"

namespace asylo {

// VerifyingKey Tests.
using VerifyingKeyFactory =
    std::function<StatusOr<std::unique_ptr<VerifyingKey>>(ByteContainerView)>;

template <class T>
using VerifyingKeyTest = T;

TYPED_TEST_SUITE_P(VerifyingKeyTest);

struct VerifyingKeyParam {
  VerifyingKeyFactory factory;
  std::string key_data;
};

template <typename T>
class EcdsaVerifyingKeyTest : public ::testing::Test {
 public:
  /* The parameters for EcdsaVerifyingKeyTest should be as follows:
   *   verifying_key_der       - a DER-encoded verifying key.
   *   verifying_key_pem       - the PEM-encoded equivalent of
   *                             verifying_key_der.
   *   verifying_key_der_proto - an AsymmetricSigningKeyProto containing
   *                             verifying_key_der.
   *   verifying_key_pem_proto - an AsymmetricSigningKeyProto containing
   *                             verifying_key_pem.
   *   other_verifying_key_pem - a different PEM-encoded verifying key.
   *   test_message_hex        - the contents of a message to be signed.
   *   signature_hex           - the signature generated by signing
   *                             test_message_hex with the signing key
   *                             corresponding to verifying_key_der.
   *   signature_r_hex         - the R component of signature_hex.
   *   signature_s_hex         - the S component of signature_hex.
   *   invalid_signature_hex   - a signature that is invalid for the given
   *                             scheme.
   *   bad_group               - incorrect NID group.
   *   sig_scheme              - the associated enum value from SignatureScheme.
   */
  EcdsaVerifyingKeyTest(
      std::string verifying_key_der, std::string verifying_key_pem,
      std::string verifying_key_der_proto, std::string verifying_key_pem_proto,
      std::string other_verifying_key_pem, std::string test_message_hex,
      std::string signature_hex, std::string signature_r_hex,
      std::string signature_s_hex, std::string invalid_signature_hex,
      int bad_group, SignatureScheme sig_scheme)
      : verifying_key_der_(verifying_key_der),
        verifying_key_pem_(verifying_key_pem),
        verifying_key_der_proto_(verifying_key_der_proto),
        verifying_key_pem_proto_(verifying_key_pem_proto),
        other_verifying_key_pem_(other_verifying_key_pem),
        test_message_hex_(test_message_hex),
        signature_hex_(signature_hex),
        signature_r_hex_(signature_r_hex),
        signature_s_hex_(signature_s_hex),
        invalid_signature_hex_(invalid_signature_hex),
        bad_group_(bad_group),
        sig_scheme_(sig_scheme) {
    verifying_key_params_.emplace_back(VerifyingKeyParam(
        {.factory = absl::bind_front(&EcdsaVerifyingKeyTest::DerFactory, this),
         .key_data = absl::HexStringToBytes(verifying_key_der)}));
    verifying_key_params_.emplace_back(VerifyingKeyParam(
        {.factory = absl::bind_front(&EcdsaVerifyingKeyTest::PemFactory, this),
         .key_data = verifying_key_pem}));
  }

  virtual StatusOr<std::unique_ptr<VerifyingKey>> DerFactory(
      ByteContainerView serialized_key) = 0;
  virtual StatusOr<std::unique_ptr<VerifyingKey>> PemFactory(
      ByteContainerView serialized_key) = 0;

  Signature CreateValidSignatureForTestMessage() {
    Signature signature;
    signature.set_signature_scheme(sig_scheme_);
    signature.mutable_ecdsa_signature()->set_r(
        absl::HexStringToBytes(signature_r_hex_));
    signature.mutable_ecdsa_signature()->set_s(
        absl::HexStringToBytes(signature_s_hex_));
    return signature;
  }

 protected:
  std::vector<VerifyingKeyParam> verifying_key_params_;
  uint8_t bad_key_[8] = {'b', 'a', 'd', ' ', 'k', 'e', 'y', 0};
  std::string verifying_key_der_;
  std::string verifying_key_pem_;
  std::string verifying_key_der_proto_;
  std::string verifying_key_pem_proto_;
  std::string other_verifying_key_pem_;
  std::string test_message_hex_;
  std::string signature_hex_;
  std::string signature_r_hex_;
  std::string signature_s_hex_;
  std::string invalid_signature_hex_;
  int bad_group_;
  SignatureScheme sig_scheme_;
};

void CheckPemKeyProtoResult(StatusOr<AsymmetricSigningKeyProto> actual_result,
                            AsymmetricSigningKeyProto expected) {
  AsymmetricSigningKeyProto actual;
  ASYLO_ASSERT_OK_AND_ASSIGN(actual, actual_result);
  ASSERT_THAT(actual.encoding(), ASYMMETRIC_KEY_PEM);
  EXPECT_EQ(actual.key_type(), expected.key_type());
  EXPECT_EQ(actual.signature_scheme(), expected.signature_scheme());
  EXPECT_THAT(actual.key(), EqualIgnoreWhiteSpace(expected.key()));
}

// Verify that Create() fails when the key has an incorrect group.
TYPED_TEST_P(VerifyingKeyTest, CreateVerifyingKeyWithBadGroupFails) {
  bssl::UniquePtr<EC_KEY> bad_key(EC_KEY_new_by_curve_name(this->bad_group_));
  ASSERT_EQ(EC_KEY_generate_key(bad_key.get()), 1);
  ASSERT_THAT(TestFixture::VerifyingKeyType::Create(std::move(bad_key)),
              Not(IsOk()));
}

// Verify that CreateFromProto() fails when the signature scheme is
// incorrect.
TYPED_TEST_P(VerifyingKeyTest,
             VerifyingKeyCreateFromProtoUnknownBadSignatureSchemeFails) {
  AsymmetricSigningKeyProto key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      this->verifying_key_pem_proto_, &key_proto));
  key_proto.set_signature_scheme(UNKNOWN_SIGNATURE_SCHEME);

  EXPECT_THAT(TestFixture::VerifyingKeyType::CreateFromProto(key_proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

// Verify that CreateFromProto() fails when the key type is incorrect.
TYPED_TEST_P(VerifyingKeyTest,
             VerifyingKeyCreateFromProtoWithSigningKeyTypeFails) {
  AsymmetricSigningKeyProto key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      this->verifying_key_pem_proto_, &key_proto));
  key_proto.set_key_type(AsymmetricSigningKeyProto::SIGNING_KEY);

  EXPECT_THAT(TestFixture::VerifyingKeyType::CreateFromProto(key_proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

// Verify that CreateFromProto() fails when the key encoding is invalid.
TYPED_TEST_P(VerifyingKeyTest,
             VerifyingKeyCreateFromProtoWithUnknownEncodingFails) {
  AsymmetricSigningKeyProto key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      this->verifying_key_pem_proto_, &key_proto));
  key_proto.set_encoding(UNKNOWN_ASYMMETRIC_KEY_ENCODING);

  EXPECT_THAT(TestFixture::VerifyingKeyType::CreateFromProto(key_proto),
              StatusIs(absl::StatusCode::kUnimplemented));
}

// Verify that CreateFromProto() fails when the key does not match the encoding.
TYPED_TEST_P(VerifyingKeyTest,
             VerifyingKeyCreateFromProtoWithMismatchedEncodingFails) {
  AsymmetricSigningKeyProto pem_key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      this->verifying_key_pem_proto_, &pem_key_proto));
  pem_key_proto.set_encoding(ASYMMETRIC_KEY_DER);

  EXPECT_THAT(TestFixture::VerifyingKeyType::CreateFromProto(pem_key_proto),
              StatusIs(absl::StatusCode::kInternal));
}

// Verify that keys created from CreateFromProto() match equivalent keys
// created from CreateFromPem and CreateFromDer.
TYPED_TEST_P(VerifyingKeyTest, VerifyingKeyCreateFromProtoSuccess) {
  std::unique_ptr<VerifyingKey> expected_pem_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      expected_pem_key,
      TestFixture::VerifyingKeyType::CreateFromPem(this->verifying_key_pem_));

  AsymmetricSigningKeyProto pem_key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      this->verifying_key_pem_proto_, &pem_key_proto));
  std::unique_ptr<VerifyingKey> pem_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      pem_key, TestFixture::VerifyingKeyType::CreateFromProto(pem_key_proto));
  EXPECT_EQ(*pem_key, *expected_pem_key);

  std::unique_ptr<VerifyingKey> expected_der_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      expected_der_key, TestFixture::VerifyingKeyType::CreateFromDer(
                            absl::HexStringToBytes(this->verifying_key_der_)));

  AsymmetricSigningKeyProto der_key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      this->verifying_key_der_proto_, &der_key_proto));
  std::unique_ptr<VerifyingKey> der_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      der_key, TestFixture::VerifyingKeyType::CreateFromProto(der_key_proto));
  EXPECT_EQ(*der_key, *expected_der_key);
}

// Verify that creating a key from an invalid encoding fails.
TYPED_TEST_P(VerifyingKeyTest,
             CreateVerifyingKeyFromInvalidSerializationFails) {
  std::vector<uint8_t> serialized_key(this->bad_key_,
                                      this->bad_key_ + sizeof(this->bad_key_));

  for (auto &param : this->verifying_key_params_) {
    EXPECT_THAT(param.factory(serialized_key), Not(IsOk()));
  }
}

// Verify that an EcdsaVerifyingKey produces an equivalent
// DER-encoding through SerializeToDer().
TYPED_TEST_P(VerifyingKeyTest, VerifyingKeySerializeToDer) {
  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_THAT(param_key->SerializeToDer(),
                IsOkAndHolds(absl::HexStringToBytes(this->verifying_key_der_)));
  }
}

TYPED_TEST_P(VerifyingKeyTest, VerifyingKeySerializeToPem) {
  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_THAT(param_key->SerializeToPem(),
                IsOkAndHolds(EqualIgnoreWhiteSpace(this->verifying_key_pem_)));
  }
}

TYPED_TEST_P(VerifyingKeyTest, SerializeToKeyProtoUnknownFailure) {
  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_THAT(param_key->SerializeToKeyProto(UNKNOWN_ASYMMETRIC_KEY_ENCODING),
                StatusIs(absl::StatusCode::kInvalidArgument));
  }
}

TYPED_TEST_P(VerifyingKeyTest, VerifyingKeySerializeToKeyProtoSuccess) {
  for (auto &param : this->verifying_key_params_) {
    AsymmetricSigningKeyProto expected_der_key_proto;
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        this->verifying_key_der_proto_, &expected_der_key_proto));

    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_THAT(param_key->SerializeToKeyProto(ASYMMETRIC_KEY_DER),
                IsOkAndHolds(EqualsProto(expected_der_key_proto)));

    AsymmetricSigningKeyProto expected_pem_key_proto;
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        this->verifying_key_pem_proto_, &expected_pem_key_proto));

    CheckPemKeyProtoResult(param_key->SerializeToKeyProto(ASYMMETRIC_KEY_PEM),
                           expected_pem_key_proto);
  }
}

// Verify that an EcdsaVerifyingKey verifies a valid signature.
TYPED_TEST_P(VerifyingKeyTest, VerifySuccess) {
  std::string valid_signature(absl::HexStringToBytes(this->signature_hex_));
  std::string valid_message(absl::HexStringToBytes(this->test_message_hex_));

  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    ASYLO_EXPECT_OK(param_key->Verify(valid_message, valid_signature));
  }
}

// Verify that an EcdsaVerifyingKey does not verify an invalid
// signature.
TYPED_TEST_P(VerifyingKeyTest, VerifyWithIncorrectSignatureFails) {
  std::string invalid_signature(
      absl::HexStringToBytes(this->invalid_signature_hex_));
  std::string valid_message(absl::HexStringToBytes(this->test_message_hex_));

  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_THAT(param_key->Verify(valid_message, invalid_signature),
                Not(IsOk()));
  }
}

// Verify that Verify() with Signature overload does not verify a signature
// with an incorrect signature scheme.
TYPED_TEST_P(VerifyingKeyTest, VerifyWithIncorrectSignatureSchemeFails) {
  std::string valid_message(absl::HexStringToBytes(this->test_message_hex_));

  Signature signature = this->CreateValidSignatureForTestMessage();
  signature.set_signature_scheme(UNKNOWN_SIGNATURE_SCHEME);

  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_THAT(param_key->Verify(valid_message, signature),
                StatusIs(absl::StatusCode::kInvalidArgument));
  }
}

// Verify that Verify() with Signature overload does not verify a signature
// without an ECDSA signature value.
TYPED_TEST_P(VerifyingKeyTest, VerifyWithMissingEcdsaSignatureFails) {
  std::string valid_message(absl::HexStringToBytes(this->test_message_hex_));

  Signature signature = this->CreateValidSignatureForTestMessage();
  signature.clear_ecdsa_signature();
  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_THAT(param_key->Verify(valid_message, signature),
                StatusIs(absl::StatusCode::kInvalidArgument));
  }
}

// Verify that Verify() with Signature overload fails without an R field.
TYPED_TEST_P(VerifyingKeyTest, VerifyWithMissingRFieldFails) {
  Signature signature = this->CreateValidSignatureForTestMessage();
  signature.mutable_ecdsa_signature()->clear_r();

  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      verifying_key,
      TestFixture::VerifyingKeyType::CreateFromPem(this->verifying_key_pem_));

  EXPECT_THAT(verifying_key->Verify(
                  absl::HexStringToBytes(this->test_message_hex_), signature),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

// Verify that Verify() with Signature overload fails without an S field.
TYPED_TEST_P(VerifyingKeyTest, VerifyWithMissingSFieldFails) {
  Signature signature = this->CreateValidSignatureForTestMessage();
  signature.mutable_ecdsa_signature()->clear_s();

  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      verifying_key,
      TestFixture::VerifyingKeyType::CreateFromPem(this->verifying_key_pem_));

  EXPECT_THAT(verifying_key->Verify(
                  absl::HexStringToBytes(this->test_message_hex_), signature),
              StatusIs(absl::StatusCode::kInvalidArgument));
}
// Verify that Verify() with Signature overload fails a short R field.
TYPED_TEST_P(VerifyingKeyTest, VerifyWithShortRFieldFails) {
  Signature signature = this->CreateValidSignatureForTestMessage();
  signature.mutable_ecdsa_signature()->set_r("too short");

  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      verifying_key,
      TestFixture::VerifyingKeyType::CreateFromPem(this->verifying_key_pem_));

  EXPECT_THAT(verifying_key->Verify(
                  absl::HexStringToBytes(this->test_message_hex_), signature),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

// Verify that Verify() with Signature overload fails with a long S field.
TYPED_TEST_P(VerifyingKeyTest, VerifyWithLongSFieldFails) {
  Signature signature = this->CreateValidSignatureForTestMessage();
  signature.mutable_ecdsa_signature()->set_s(
      "this is an s field that is way too long");

  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      verifying_key,
      TestFixture::VerifyingKeyType::CreateFromPem(this->verifying_key_pem_));

  EXPECT_THAT(verifying_key->Verify(
                  absl::HexStringToBytes(this->test_message_hex_), signature),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

// Verify that Verify() with Signature overload passes with valid signature.
TYPED_TEST_P(VerifyingKeyTest, VerifySignatureOverloadSuccess) {
  Signature signature = this->CreateValidSignatureForTestMessage();

  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      verifying_key,
      TestFixture::VerifyingKeyType::CreateFromPem(this->verifying_key_pem_));

  ASYLO_EXPECT_OK(verifying_key->Verify(
      absl::HexStringToBytes(this->test_message_hex_), signature));
}

// Verify that operator== fails with a different VerifyingKey implementation.
TYPED_TEST_P(VerifyingKeyTest, EqualsFailsWithDifferentClassKeys) {
  FakeVerifyingKey other_verifying_key(this->sig_scheme_,
                                       this->verifying_key_der_);

  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_FALSE(*(param_key) == other_verifying_key);
  }
}

// Verify that operator!= passes with a different VerifyingKey
TYPED_TEST_P(VerifyingKeyTest, NotEqualsPassesWithDifferentClassKeys) {
  FakeVerifyingKey other_verifying_key(this->sig_scheme_,
                                       this->verifying_key_der_);

  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_TRUE(*(param_key) != other_verifying_key);
  }
}

// Verify that operator== passes when given a key created with the same data.
TYPED_TEST_P(VerifyingKeyTest, EqualsSucceedsWithEquivalentKeys) {
  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));
    std::unique_ptr<VerifyingKey> other_verifying_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(other_verifying_key,
                               param.factory(param.key_data));

    EXPECT_TRUE(*(param_key) == *other_verifying_key);
  }
}

// Verify that operator== fails when given a key created with different data.
TYPED_TEST_P(VerifyingKeyTest, EqualsFailsWithDifferentKeys) {
  std::unique_ptr<VerifyingKey> other_verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(other_verifying_key,
                             TestFixture::VerifyingKeyType::CreateFromPem(
                                 this->other_verifying_key_pem_));

  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_FALSE(*(param_key) == *other_verifying_key);
  }
}

// Verify that operator!= fails when given a key created with the same data.
TYPED_TEST_P(VerifyingKeyTest, NotEqualsFailsWithEquivalentKeys) {
  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));
    std::unique_ptr<VerifyingKey> other_verifying_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(other_verifying_key,
                               param.factory(param.key_data));

    EXPECT_FALSE(*(param_key) != *other_verifying_key);
  }
}

// Verify that operator!= passes when given a key created with different
TYPED_TEST_P(VerifyingKeyTest, NotEqualsSucceedsWithDifferentKeys) {
  std::unique_ptr<VerifyingKey> other_verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(other_verifying_key,
                             TestFixture::VerifyingKeyType::CreateFromPem(
                                 this->other_verifying_key_pem_));
  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_TRUE(*(param_key) != *other_verifying_key);
  }
}

// Verify that GetSignatureScheme() indicates the expected scheme for the key
// type.
TYPED_TEST_P(VerifyingKeyTest, SignatureScheme) {
  for (auto &param : this->verifying_key_params_) {
    std::unique_ptr<VerifyingKey> param_key;
    ASYLO_ASSERT_OK_AND_ASSIGN(param_key, param.factory(param.key_data));

    EXPECT_EQ(param_key->GetSignatureScheme(), this->sig_scheme_);
  }
}

REGISTER_TYPED_TEST_SUITE_P(
    VerifyingKeyTest, CreateVerifyingKeyWithBadGroupFails,
    VerifyingKeyCreateFromProtoUnknownBadSignatureSchemeFails,
    VerifyingKeyCreateFromProtoWithSigningKeyTypeFails,
    VerifyingKeyCreateFromProtoWithUnknownEncodingFails,
    VerifyingKeyCreateFromProtoWithMismatchedEncodingFails,
    VerifyingKeyCreateFromProtoSuccess,
    CreateVerifyingKeyFromInvalidSerializationFails, VerifyingKeySerializeToDer,
    VerifyingKeySerializeToPem, SerializeToKeyProtoUnknownFailure,
    VerifyingKeySerializeToKeyProtoSuccess, VerifySuccess,
    VerifyWithIncorrectSignatureFails, VerifyWithIncorrectSignatureSchemeFails,
    VerifyWithMissingEcdsaSignatureFails, VerifyWithMissingRFieldFails,
    VerifyWithMissingSFieldFails, VerifyWithShortRFieldFails,
    VerifyWithLongSFieldFails, VerifySignatureOverloadSuccess,
    EqualsFailsWithDifferentClassKeys, NotEqualsPassesWithDifferentClassKeys,
    EqualsSucceedsWithEquivalentKeys, EqualsFailsWithDifferentKeys,
    NotEqualsFailsWithEquivalentKeys, NotEqualsSucceedsWithDifferentKeys,
    SignatureScheme);

// Signing Key Tests.
template <class T>
using SigningKeyTest = T;

TYPED_TEST_SUITE_P(SigningKeyTest);

template <typename T>
class EcdsaSigningKeyTest : public ::testing::Test {
 public:
  /* The parameters for EcdsaSigningKeyTest should be as follows:
   *   signing_key_der       - a DER-encoded signing key.
   *   signing_key_pem       - the PEM-encoded equivalent of signing_key_der.
   *   signing_key_der_proto - an AsymmetricKeyProto containing signing_key_der.
   *   signing_key_pem_proto - an AsymmetricKeyProto containing signing_key_pem.
   *   test_message_hex      - the contents of a message to be signed.
   *   bad_group             - incorrect NID group.
   *   message_size          - the size of messages to be signed, used to
   *                           dynamically generate messages for tests.
   *   sig_scheme            - the associated enum value from SignatureScheme.
   */
  EcdsaSigningKeyTest(std::string signing_key_der, std::string signing_key_pem,
                      std::string signing_key_der_proto,
                      std::string signing_key_pem_proto,
                      std::string test_message_hex, int bad_group,
                      int message_size, SignatureScheme sig_scheme)
      : signing_key_der_(signing_key_der),
        signing_key_pem_(signing_key_pem),
        signing_key_der_proto_(signing_key_der_proto),
        signing_key_pem_proto_(signing_key_pem_proto),
        test_message_hex_(test_message_hex),
        bad_group_(bad_group),
        message_size_(message_size),
        sig_scheme_(sig_scheme) {}

  void SetUp() override {
    ASYLO_ASSERT_OK_AND_ASSIGN(signing_key_, T::Create());

    CleansingVector<uint8_t> serialized;
    ASYLO_ASSERT_OK_AND_ASSIGN(serialized, signing_key_->SerializeToDer());

    LOG(INFO) << "Using random SigningKey: "
              << absl::BytesToHexString(
                     CopyToByteContainer<std::string>(serialized));
  }

  std::unique_ptr<T> signing_key_;
  uint8_t bad_key_[8] = {'b', 'a', 'd', ' ', 'k', 'e', 'y', 0};

  std::string signing_key_der_;
  std::string signing_key_pem_;
  std::string signing_key_der_proto_;
  std::string signing_key_pem_proto_;
  std::string test_message_hex_;
  int bad_group_;
  int message_size_;
  SignatureScheme sig_scheme_;
};

// Verify that CreateFromProto() fails when the signature scheme is incorrect.
TYPED_TEST_P(SigningKeyTest,
             SigningKeyCreateFromProtoWithUnknownSignatureSchemeFails) {
  AsymmetricSigningKeyProto key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(this->signing_key_pem_proto_,
                                                  &key_proto));
  key_proto.set_signature_scheme(UNKNOWN_SIGNATURE_SCHEME);

  EXPECT_THAT(TestFixture::SigningKeyType::CreateFromProto(key_proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

// Verify that CreateFromProto() fails when the key type is incorrect.
TYPED_TEST_P(SigningKeyTest,
             SigningKeyCreateFromProtoWithVerifyingKeyTypeFails) {
  AsymmetricSigningKeyProto key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(this->signing_key_pem_proto_,
                                                  &key_proto));
  key_proto.set_key_type(AsymmetricSigningKeyProto::VERIFYING_KEY);

  EXPECT_THAT(TestFixture::SigningKeyType::CreateFromProto(key_proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

// Verify that CreateFromProto() fails when the key encoding is invalid.
TYPED_TEST_P(SigningKeyTest,
             SigningKeyCreateFromProtoWithUnknownEncodingFails) {
  AsymmetricSigningKeyProto key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(this->signing_key_pem_proto_,
                                                  &key_proto));
  key_proto.set_encoding(UNKNOWN_ASYMMETRIC_KEY_ENCODING);

  EXPECT_THAT(TestFixture::SigningKeyType::CreateFromProto(key_proto),
              StatusIs(absl::StatusCode::kUnimplemented));
}

// Verify that CreateFromProto() fails when the key does not match the
// encoding.
TYPED_TEST_P(SigningKeyTest,
             SigningKeyCreateFromProtoWithMismatchedEncodingFails) {
  AsymmetricSigningKeyProto pem_key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(this->signing_key_pem_proto_,
                                                  &pem_key_proto));
  pem_key_proto.set_encoding(ASYMMETRIC_KEY_DER);

  EXPECT_THAT(TestFixture::SigningKeyType::CreateFromProto(pem_key_proto),
              StatusIs(absl::StatusCode::kInternal));
}

TYPED_TEST_P(SigningKeyTest, SigningKeyCreateFromProtoSuccess) {
  AsymmetricSigningKeyProto pem_key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(this->signing_key_pem_proto_,
                                                  &pem_key_proto));
  ASYLO_EXPECT_OK(TestFixture::SigningKeyType::CreateFromProto(pem_key_proto));

  AsymmetricSigningKeyProto der_key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(this->signing_key_pem_proto_,
                                                  &der_key_proto));
  ASYLO_EXPECT_OK(TestFixture::SigningKeyType::CreateFromProto(der_key_proto));
}

// Verify that Create() fails when the key has an incorrect group.
TYPED_TEST_P(SigningKeyTest, CreateSigningKeyWithBadGroupFails) {
  bssl::UniquePtr<EC_KEY> bad_key(EC_KEY_new_by_curve_name(this->bad_group_));
  ASSERT_TRUE(EC_KEY_generate_key(bad_key.get()));
  ASSERT_THAT(TestFixture::SigningKeyType::Create(std::move(bad_key)),
              Not(IsOk()));
}

// Verify that GetSignatureScheme() indicates the expected scheme for the key
// type.
TYPED_TEST_P(SigningKeyTest, SignatureScheme) {
  EXPECT_EQ(this->signing_key_->GetSignatureScheme(), this->sig_scheme_);
}

// Verify that an EcdsaSigningKey created from a PEM-encoded key
// serializes to the correct DER-encoding.
TYPED_TEST_P(SigningKeyTest, CreateSigningKeyFromPemMatchesDer) {
  std::unique_ptr<SigningKey> signing_key_pem;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      signing_key_pem,
      TestFixture::SigningKeyType::CreateFromPem(this->signing_key_pem_));

  CleansingVector<uint8_t> serialized_der;
  ASYLO_ASSERT_OK_AND_ASSIGN(serialized_der, signing_key_pem->SerializeToDer());

  EXPECT_EQ(ByteContainerView(serialized_der),
            ByteContainerView(absl::HexStringToBytes(this->signing_key_der_)));
}

TYPED_TEST_P(SigningKeyTest, CreateSigningKeyFromDerMatchesPem) {
  std::unique_ptr<SigningKey> signing_key_der;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      signing_key_der, TestFixture::SigningKeyType::CreateFromDer(
                           absl::HexStringToBytes(this->signing_key_der_)));

  CleansingVector<char> serialized_pem;
  ASYLO_ASSERT_OK_AND_ASSIGN(serialized_pem, signing_key_der->SerializeToPem());

  EXPECT_THAT(CopyToByteContainer<std::string>(serialized_pem),
              EqualIgnoreWhiteSpace(this->signing_key_pem_));
}

TYPED_TEST_P(SigningKeyTest, SerializeToKeyProtoUnknownFailure) {
  EXPECT_THAT(
      this->signing_key_->SerializeToKeyProto(UNKNOWN_ASYMMETRIC_KEY_ENCODING),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TYPED_TEST_P(SigningKeyTest, SerializeToKeyProtoSuccess) {
  std::unique_ptr<SigningKey> signing_key_der;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      signing_key_der, TestFixture::SigningKeyType::CreateFromDer(
                           absl::HexStringToBytes(this->signing_key_der_)));

  AsymmetricSigningKeyProto expected_der_key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(this->signing_key_der_proto_,
                                                  &expected_der_key_proto));

  EXPECT_THAT(signing_key_der->SerializeToKeyProto(ASYMMETRIC_KEY_DER),
              IsOkAndHolds(EqualsProto(expected_der_key_proto)));

  std::unique_ptr<SigningKey> signing_key_pem;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      signing_key_pem,
      TestFixture::SigningKeyType::CreateFromPem(this->signing_key_pem_));

  AsymmetricSigningKeyProto expected_pem_key_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(this->signing_key_pem_proto_,
                                                  &expected_pem_key_proto));

  CheckPemKeyProtoResult(
      signing_key_pem->SerializeToKeyProto(ASYMMETRIC_KEY_PEM),
      expected_pem_key_proto);
}

// Verify that a randomly-generated EcdsaSigningKey can produce a
// signature that the corresponding EcdsaVerifyingKey can verify.
TYPED_TEST_P(SigningKeyTest, SignAndVerify) {
  std::vector<uint8_t> message(this->message_size_);
  ASSERT_TRUE(RAND_bytes(message.data(), this->message_size_));

  std::vector<uint8_t> signature;
  ASYLO_ASSERT_OK(this->signing_key_->Sign(message, &signature));

  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(verifying_key,
                             this->signing_key_->GetVerifyingKey());
  ASYLO_EXPECT_OK(verifying_key->Verify(message, signature));

  // Ensure that the signature is not verifiable if one bit is flipped.
  signature.back() ^= 1;
  EXPECT_THAT(verifying_key->Verify(message, signature), Not(IsOk()));
}

// Verifies that Sign and Verify work with the Signature overloads.
TYPED_TEST_P(SigningKeyTest, SignAndVerifySignatureOverloads) {
  std::string message(absl::HexStringToBytes(this->test_message_hex_));
  Signature signature;
  ASYLO_ASSERT_OK(this->signing_key_->Sign(message, &signature));

  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(verifying_key,
                             this->signing_key_->GetVerifyingKey());

  ASYLO_EXPECT_OK(verifying_key->Verify(message, signature));

  // Ensure that signature is not verifiable if one bit is flipped.
  signature.mutable_ecdsa_signature()->mutable_r()->back() ^= 1;
  EXPECT_THAT(verifying_key->Verify(message, signature), Not(IsOk()));
}

// Verify that SerializeToDer() and CreateFromDer() from a serialized key are
// working correctly, and that an EcdsaSigningKey restored from a
// serialized version of another EcdsaSigningKey can verify a
// signature produced by the original key successfully.
TYPED_TEST_P(SigningKeyTest, SerializeToDerAndRestoreSigningKey) {
  CleansingVector<uint8_t> serialized_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(serialized_key,
                             this->signing_key_->SerializeToDer());

  auto signing_key_result2 =
      TestFixture::SigningKeyType::CreateFromDer(serialized_key);
  ASYLO_ASSERT_OK(signing_key_result2);

  std::unique_ptr<typename TestFixture::SigningKeyType> signing_key2 =
      std::move(signing_key_result2).value();

  // Try to verify something signed by the original key.
  std::vector<uint8_t> message(this->message_size_);
  ASSERT_TRUE(RAND_bytes(message.data(), this->message_size_));

  std::vector<uint8_t> signature;
  ASYLO_ASSERT_OK(this->signing_key_->Sign(message, &signature));

  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSERT_OK_AND_ASSIGN(verifying_key, signing_key2->GetVerifyingKey());

  ASYLO_EXPECT_OK(verifying_key->Verify(message, signature));
}

// Verify that an EcdsaSigningKey created from a serialized key
// produces the same serialization as the one it was constructed from.
TYPED_TEST_P(SigningKeyTest, RestoreFromAndSerializeToDerSigningKey) {
  std::string serialized_key_hex(
      absl::HexStringToBytes(this->signing_key_der_));
  CleansingVector<uint8_t> serialized_key_bin_expected =
      CopyToByteContainer<CleansingVector<uint8_t>>(serialized_key_hex);

  auto signing_key_result2 =
      TestFixture::SigningKeyType::CreateFromDer(serialized_key_bin_expected);
  ASYLO_ASSERT_OK(signing_key_result2);

  std::unique_ptr<typename TestFixture::SigningKeyType> signing_key2 =
      std::move(signing_key_result2).value();

  CleansingVector<uint8_t> serialized_key_bin_actual;
  ASYLO_ASSERT_OK_AND_ASSIGN(serialized_key_bin_actual,
                             signing_key2->SerializeToDer());

  EXPECT_EQ(serialized_key_bin_expected, serialized_key_bin_actual);
}

// Verify that creating an EcdsaSigningKey from an invalid DER
// serialization fails.
TYPED_TEST_P(SigningKeyTest, CreateSigningKeyFromInvalidDerSerializationFails) {
  std::vector<uint8_t> serialized_key(this->bad_key_,
                                      this->bad_key_ + sizeof(this->bad_key_));

  EXPECT_THAT(TestFixture::SigningKeyType::CreateFromDer(serialized_key),
              Not(IsOk()));
}

// Verify that creating an EcdsaSigningKey from an invalid PEM
// serialization fails.
TYPED_TEST_P(SigningKeyTest, CreateSigningKeyFromInvalidPemSerializationFails) {
  std::vector<uint8_t> serialized_key(this->bad_key_,
                                      this->bad_key_ + sizeof(this->bad_key_));

  EXPECT_THAT(TestFixture::SigningKeyType::CreateFromPem(serialized_key),
              Not(IsOk()));
}

// Verify that we can export and import the public key coordinate.
TYPED_TEST_P(SigningKeyTest, ExportAndImportRawPublicKey) {
  // First export and import key point
  typename TestFixture::CurvePointType public_key_point;
  ASYLO_ASSERT_OK_AND_ASSIGN(public_key_point,
                             this->signing_key_->GetPublicKeyPoint());

  std::unique_ptr<VerifyingKey> verifier;
  ASYLO_ASSERT_OK_AND_ASSIGN(
      verifier, TestFixture::VerifyingKeyType::Create(public_key_point));

  // Second, ensure the verifying key can check signatures properly.
  std::vector<uint8_t> signature;
  ASYLO_EXPECT_OK(this->signing_key_->Sign("sign this stuff", &signature));
  ASYLO_EXPECT_OK(verifier->Verify("sign this stuff", signature));
}

REGISTER_TYPED_TEST_SUITE_P(
    SigningKeyTest, SigningKeyCreateFromProtoWithUnknownSignatureSchemeFails,
    SigningKeyCreateFromProtoWithVerifyingKeyTypeFails,
    SigningKeyCreateFromProtoWithUnknownEncodingFails,
    SigningKeyCreateFromProtoWithMismatchedEncodingFails,
    SigningKeyCreateFromProtoSuccess, CreateSigningKeyWithBadGroupFails,
    SignatureScheme, CreateSigningKeyFromPemMatchesDer,
    CreateSigningKeyFromDerMatchesPem, SerializeToKeyProtoUnknownFailure,
    SerializeToKeyProtoSuccess, SignAndVerify, SignAndVerifySignatureOverloads,
    SerializeToDerAndRestoreSigningKey, RestoreFromAndSerializeToDerSigningKey,
    CreateSigningKeyFromInvalidDerSerializationFails,
    CreateSigningKeyFromInvalidPemSerializationFails,
    ExportAndImportRawPublicKey);

}  // namespace asylo

#endif  // ASYLO_CRYPTO_ECDSA_SIGNING_KEY_TEST_H_
