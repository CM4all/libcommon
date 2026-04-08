// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "MemBio.hxx"
#include "Error.hxx"

UniqueBIO
BIO_new_mem_buf(std::span<const std::byte> buffer)
{
	BIO *bio = BIO_new_mem_buf(buffer.data(), static_cast<int>(buffer.size()));
	if (bio == nullptr)
		throw SslError{"BIO_new_mem_buf() failed"};

	return UniqueBIO{bio};
}
