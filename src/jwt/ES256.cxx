// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ES256.hxx"
#include "lib/sodium/Base64.hxx"
#include "lib/sodium/SHA256.hxx"
#include "lib/openssl/Error.hxx"
#include "lib/openssl/UniqueEVP.hxx"
#include "lib/openssl/UniqueEC.hxx"
#include "lib/openssl/AllocateSign.hxx"
#include "util/AllocatedString.hxx"
#include "util/SpanCast.hxx"

#include <openssl/evp.h>
#include <openssl/ec.h>

namespace JWT {

/**
 * Create a JWT-ES256 signature.
 *
 * Throws on (OpenSSL/libcrypto) error.
 *
 * @param header_dot_payload the concatentation of
 * UrlSafeBase64(header) "." UrlSafeBase64(payload)
 * @return UrlSafeBase64 of the RSA signature
 */
static AllocatedString
SignES256(EVP_PKEY &key, const SHA256DigestView digest)
{
	UniqueEVP_PKEY_CTX ctx(EVP_PKEY_CTX_new(&key, nullptr));
	if (!ctx)
		throw SslError("EVP_PKEY_CTX_new() failed");

	if (EVP_PKEY_sign_init(ctx.get()) <= 0)
		throw SslError("EVP_PKEY_sign_init() failed");

	if (EVP_PKEY_CTX_set_signature_md(ctx.get(), EVP_sha256()) <= 0)
		throw SslError("EVP_PKEY_CTX_set_signature_md() failed");

	const auto ossl_sig = EVP_PKEY_sign(*ctx, digest);

	auto ossl_sig_data = reinterpret_cast<const unsigned char *>(ossl_sig.data());
	const UniqueECDSA_SIG esig{d2i_ECDSA_SIG(nullptr, &ossl_sig_data,
						 ossl_sig.size())};
        if (esig == nullptr)
		throw SslError{"d2i_ECDSA_SIG() failed"};

	const BIGNUM *r, *s;
	ECDSA_SIG_get0(esig.get(), &r, &s);

	const std::size_t r_size = BN_num_bytes(r), s_size = BN_num_bytes(s);
	AllocatedArray<std::byte> sig{r_size + s_size};

	BN_bn2bin(r, reinterpret_cast<unsigned char *>(sig.data()));
	BN_bn2bin(s, reinterpret_cast<unsigned char *>(sig.data() + r_size));

	return UrlSafeBase64(sig);
}

AllocatedString
SignES256(EVP_PKEY &key, std::string_view protected_header_b64,
	  std::string_view payload_b64)
{
	SHA256State sha256;
	sha256.Update(AsBytes(protected_header_b64));
	sha256.Update(AsBytes(std::string_view{"."}));
	sha256.Update(AsBytes(payload_b64));

	return SignES256(key, sha256.Final());
}

} // namespace JWT
