// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "EdDSA.hxx"
#include "lib/sodium/Base64.hxx"
#include "util/AllocatedArray.hxx"
#include "util/AllocatedString.hxx"
#include "util/StringSplit.hxx"

#include <sodium/crypto_sign.h>

#include <string>

namespace JWT {

using Ed25519Signature = std::array<std::byte, crypto_sign_BYTES>;

static AllocatedString
SignEdDSA(const CryptoSignSecretKeyView key, std::span<const std::byte> input) noexcept
{
	Ed25519Signature signature;
	crypto_sign_detached((unsigned char *)signature.data(), nullptr,
			     (const unsigned char *)input.data(), input.size(),
			     (const unsigned char *)key.data());

	return UrlSafeBase64(signature);
}

AllocatedString
SignEdDSA(const CryptoSignSecretKeyView key, std::string_view header_b64,
	  std::string_view payload_b64) noexcept
{
	/* unfortunately, we need to allocate memory here, because
	   libsodium's multi-part API is not compatible with
	   crypto_sign_detached() and cannot be used here */
	std::string input;
	input.reserve(header_b64.size() + 1 + payload_b64.size());
	input.assign(header_b64);
	input.push_back('.');
	input.append(payload_b64);

	return SignEdDSA(key, std::as_bytes(std::span{input}));
}

bool
VerifyEdDSA(const CryptoSignPublicKeyView key,
	    std::string_view header_dot_payload_b64,
	    std::string_view signature_b64) noexcept
{
	Ed25519Signature signature;

	/* subtracting 1 because the function includes space for the
	   null terminator */
	constexpr size_t expected_signature_b64_size =
		sodium_base64_ENCODED_LEN(signature.size(),
					  sodium_base64_VARIANT_URLSAFE_NO_PADDING) - 1;

	if (signature_b64.size() != expected_signature_b64_size)
		return false;

	size_t signature_size;
	if (sodium_base642bin((unsigned char *)signature.data(),
			      signature.size(),
			      signature_b64.data(), signature_b64.size(),
			      nullptr, &signature_size,
			      nullptr,
			      sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0)
		return false;

	if (signature_size != signature.size())
		return false;

	return crypto_sign_verify_detached((const unsigned char *)signature.data(),
					   (const unsigned char *)header_dot_payload_b64.data(),
					   header_dot_payload_b64.size(),
					   (const unsigned char *)key.data()) == 0;

}

bool
VerifyEdDSA(const CryptoSignPublicKeyView key,
	    std::string_view header_dot_payload_dot_signature_b64) noexcept
{
	const auto [header_dot_payload_b64, signature_b64] =
		SplitLast(header_dot_payload_dot_signature_b64, '.');

	return VerifyEdDSA(key, header_dot_payload_b64, signature_b64);
}

AllocatedArray<std::byte>
VerifyDecodeEdDSA(const CryptoSignPublicKeyView key,
		  std::string_view header_dot_payload_b64,
		  std::string_view signature_b64) noexcept
{
	if (!VerifyEdDSA(key, header_dot_payload_b64, signature_b64))
		return nullptr;

	const auto payload_b64 = Split(header_dot_payload_b64, '.').second;
	return DecodeUrlSafeBase64(payload_b64);
}

AllocatedArray<std::byte>
VerifyDecodeEdDSA(const CryptoSignPublicKeyView key,
		  std::string_view header_dot_payload_dot_signature_b64) noexcept
{
	const auto [header_dot_payload_b64, signature_b64] =
		SplitLast(header_dot_payload_dot_signature_b64, '.');

	return VerifyDecodeEdDSA(key, header_dot_payload_b64, signature_b64);
}

} // namespace JWT
