// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "RS256.hxx"
#include "lib/sodium/Base64.hxx"
#include "lib/sodium/SHA256.hxx"
#include "lib/openssl/Error.hxx"
#include "lib/openssl/UniqueEVP.hxx"
#include "lib/openssl/AllocateSign.hxx"
#include "util/AllocatedString.hxx"
#include "util/SpanCast.hxx"

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
SignRS256(EVP_PKEY &key, const SHA256DigestView digest)
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

	const auto sig = EVP_PKEY_sign(*ctx, digest);
	return UrlSafeBase64(sig);
}

AllocatedString
SignRS256(EVP_PKEY &key, std::string_view protected_header_b64,
	  std::string_view payload_b64)
{
	SHA256State sha256;
	sha256.Update(AsBytes(protected_header_b64));
	sha256.Update(AsBytes(std::string_view{"."}));
	sha256.Update(AsBytes(payload_b64));

	return SignRS256(key, sha256.Final());
}

} // namespace JWT
