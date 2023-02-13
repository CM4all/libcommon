// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <array>
#include <cstddef> // for std::byte
#include <string_view>

class AllocatedString;
template<typename T> class AllocatedArray;

namespace JWT {

// 64 = crypto_sign_SECRETKEYBYTES
using Ed25519SecretKey = std::array<std::byte, 64>;

// 32 = crypto_sign_PUBLICKEYBYTES
using Ed25519PublicKey = std::array<std::byte, 32>;

/**
 * Create an EdDSA (kty=OKP, crv=Ed25519) signature according to
 * RFC8037.
 *
 * @param header_b64 the UrlSafeBase64 of the JWT header
 * @param payload_b64 the UrlSafeBase64 of the payload
 * @return UrlSafeBase64 of the EdDSA signature
 */
AllocatedString
SignEdDSA(const Ed25519SecretKey &key, std::string_view header_b64,
	  std::string_view payload_b64) noexcept;

/**
 * Verify an EdDSA (kty=OKP, crv=Ed25519) signature according to
 * RFC8037.
 *
 * @param header_dot_payload_b64 the UrlSafeBase64 of the JWT header
 * plus a dot plus the the UrlSafeBase64 of the payload
 * @param signature_b64 the UrlSafeBase64 of the signature
 * @return true if the signature is valid
 */
[[gnu::pure]]
bool
VerifyEdDSA(const Ed25519PublicKey &key,
	    std::string_view header_dot_payload_b64,
	    std::string_view signature_b64) noexcept;

[[gnu::pure]]
bool
VerifyEdDSA(const Ed25519PublicKey &key,
	    std::string_view header_dot_payload_dot_signature_b64) noexcept;

/**
 * @return the base64-decoded payload on success or nullptr on error
 */
AllocatedArray<std::byte>
VerifyDecodeEdDSA(const Ed25519PublicKey &key,
		  std::string_view header_dot_payload_b64,
		  std::string_view signature_b64) noexcept;

/**
 * @return the base64-decoded payload on success or nullptr on error
 */
AllocatedArray<std::byte>
VerifyDecodeEdDSA(const Ed25519PublicKey &key,
		  std::string_view header_dot_payload_dot_signature_b64) noexcept;

} // namespace JWT
