// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ES256.hxx"
#include "lib/sodium/Base64Alloc.hxx"
#include "lib/sodium/SHA256.hxx"
#include "lib/openssl/BN.hxx"
#include "lib/openssl/Error.hxx"
#include "lib/openssl/UniqueEVP.hxx"
#include "lib/openssl/UniqueEC.hxx"
#include "lib/openssl/AllocateSign.hxx"
#include "util/AllocatedString.hxx"
#include "util/SpanCast.hxx"

#include <openssl/evp.h>
#include <openssl/ec.h>

#include <stdexcept>

namespace JWT {

std::array<std::byte, 64>
EncodeES256Signature(const ECDSA_SIG &esig)
{
	constexpr std::size_t es256_component_size = 32;

	const BIGNUM *r, *s;
	ECDSA_SIG_get0(&esig, &r, &s);

	if (BN_num_bytes(r) > (int)es256_component_size ||
	    BN_num_bytes(s) > (int)es256_component_size)
		throw std::invalid_argument{"ES256 signature component too large"};

	std::array<std::byte, es256_component_size * 2> sig;

	if (BN_bn2binpad(*r, std::span{sig}.first<es256_component_size>()) != (int)es256_component_size ||
	    BN_bn2binpad(*s, std::span{sig}.subspan<es256_component_size, es256_component_size>()) != (int)es256_component_size)
		throw SslError{"BN_bn2binpad() failed"};

	return sig;
}

/**
 * Create a JWT-ES256 signature.
 *
 * Throws on (OpenSSL/libcrypto) error.
 *
 * @param digest the SHA-256 digest of the signing input
 * @return UrlSafeBase64 of the JOSE signature
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

	return UrlSafeBase64(EncodeES256Signature(*esig));
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
