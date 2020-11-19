/*
 * Copyright 2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "EdDSA.hxx"
#include "sodium/Base64.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <sodium/crypto_sign.h>

#include <string>

namespace JWT {

using Ed25519Signature = std::array<std::byte, crypto_sign_BYTES>;

static AllocatedString<>
SignEdDSA(const Ed25519SecretKey &key, ConstBuffer<void> _input) noexcept
{
	const auto input = ConstBuffer<unsigned char>::FromVoid(_input);

	Ed25519Signature signature;
	crypto_sign_detached((unsigned char *)signature.data(), nullptr,
			     input.data, input.size,
			     (const unsigned char *)key.data());

	return UrlSafeBase64(ConstBuffer<void>(signature.data(),
					       signature.size()));
}

AllocatedString<>
SignEdDSA(const Ed25519SecretKey &key, std::string_view header_b64,
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

	return SignEdDSA(key, StringView(input).ToVoid());
}

bool
VerifyEdDSA(const Ed25519PublicKey &key,
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
VerifyEdDSA(const Ed25519PublicKey &key,
	    std::string_view header_dot_payload_dot_signature_b64) noexcept
{
	const StringView hps(header_dot_payload_dot_signature_b64);
	const auto [header_dot_payload_b64, signature_b64] = hps.SplitLast('.');

	return VerifyEdDSA(key, header_dot_payload_b64, signature_b64);
}

} // namespace JWT
