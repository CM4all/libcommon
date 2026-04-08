// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PemKey.hxx"
#include "Error.hxx"
#include "MemBio.hxx"
#include "util/SpanCast.hxx"

#include <openssl/pem.h>

UniqueEVP_PKEY
DecodePemPublicKey(std::string_view pem)
{
	const auto bio = BIO_new_mem_buf(AsBytes(pem));
	EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);
	if (pkey == nullptr)
		throw SslError{"PEM_read_bio_PUBKEY() failed"};

	return UniqueEVP_PKEY{pkey};
}

UniqueEVP_PKEY
DecodePemPrivateKey(std::string_view pem)
{
	const auto bio = BIO_new_mem_buf(AsBytes(pem));
	EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);
	if (pkey == nullptr)
		throw SslError{"PEM_read_bio_PrivateKey() failed"};

	return UniqueEVP_PKEY{pkey};
}
