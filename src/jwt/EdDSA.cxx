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

} // namespace JWT
