// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Hash.hxx"
#include "Buffer.hxx"
#include "Error.hxx"

#include <openssl/evp.h>

SHA1Digest
CalcSHA1(std::span<const std::byte> src)
{
	SHA1Digest result;
	if (!EVP_Digest(src.data(), src.size(),
			result.data, nullptr, EVP_sha1(),
			nullptr))
		throw SslError("EVP_Digest() failed");

	return result;
}

SHA1Digest
CalcSHA1(X509_NAME &src)
{
	const SslBuffer buffer(src);
	return CalcSHA1(buffer.get());
}
