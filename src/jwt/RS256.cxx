/*
 * Copyright 2016-2021 CM4all GmbH
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

#include "RS256.hxx"
#include "sodium/Base64.hxx"
#include "sodium/SHA256.hxx"
#include "ssl/Error.hxx"
#include "ssl/UniqueEVP.hxx"
#include "util/AllocatedString.hxx"
#include "util/StringView.hxx"

#include <openssl/rsa.h>

namespace JWT {

/**
 * Create a JWT-RS256 signature.
 *
 * Throws on (OpenSSL/libcrypto) error.
 *
 * @param header_dot_payload the concatentation of
 * UrlSafeBase64(header) "." UrlSafeBase64(payload)
 * @return UrlSafeBase64 of the RSA signature
 */
static AllocatedString
SignRS256(EVP_PKEY &key, const SHA256Digest &digest)
{
	UniqueEVP_PKEY_CTX ctx(EVP_PKEY_CTX_new(&key, nullptr));
	if (!ctx)
		throw SslError("EVP_PKEY_CTX_new() failed");

	if (EVP_PKEY_sign_init(ctx.get()) <= 0)
		throw SslError("EVP_PKEY_sign_init() failed");

	if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING) <= 0)
		throw SslError("EVP_PKEY_CTX_set_rsa_padding() failed");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

	if (EVP_PKEY_CTX_set_signature_md(ctx.get(), EVP_sha256()) <= 0)
		throw SslError("EVP_PKEY_CTX_set_signature_md() failed");

#pragma GCC diagnostic pop

	size_t length;
	if (EVP_PKEY_sign(ctx.get(), nullptr, &length,
			  (const unsigned char *)&digest, sizeof(digest)) <= 0)
		throw SslError("EVP_PKEY_sign() failed");

	std::unique_ptr<unsigned char[]> buffer(new unsigned char[length]);
	if (EVP_PKEY_sign(ctx.get(), buffer.get(), &length,
			  (const unsigned char *)&digest, sizeof(digest)) <= 0)
		throw SslError("EVP_PKEY_sign() failed");

	return UrlSafeBase64(ConstBuffer<void>(buffer.get(), length));
}

AllocatedString
SignRS256(EVP_PKEY &key, std::string_view protected_header_b64,
	  std::string_view payload_b64)
{
	SHA256State sha256;
	sha256.Update(StringView(protected_header_b64).ToVoid());
	sha256.Update(StringView(".").ToVoid());
	sha256.Update(StringView(payload_b64).ToVoid());

	return SignRS256(key, sha256.Final());
}

} // namespace JWT
