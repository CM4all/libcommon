// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Error.hxx"
#include "util/AllocatedArray.hxx"

#include <openssl/evp.h>

#include <cassert>

/**
 * Convenience wrapper for OpenSSL's EVP_PKEY_sign() which returns the
 * signature in a newly allocated buffer.
 *
 * Throws on error.
 */
inline AllocatedArray<std::byte>
EVP_PKEY_sign(EVP_PKEY_CTX &ctx, std::span<const std::byte> tbs)
{
	size_t length;
	if (EVP_PKEY_sign(&ctx, nullptr, &length,
			  reinterpret_cast<const unsigned char *>(tbs.data()),
			  tbs.size()) <= 0)
		throw SslError("EVP_PKEY_sign() failed");

	AllocatedArray<std::byte> sig{length};
	if (EVP_PKEY_sign(&ctx, reinterpret_cast<unsigned char *>(sig.data()), &length,
			  reinterpret_cast<const unsigned char *>(tbs.data()),
			  tbs.size()) <= 0)
		throw SslError("EVP_PKEY_sign() failed");

	assert(length <= sig.size());
	sig.SetSize(length);

	return sig;
}
