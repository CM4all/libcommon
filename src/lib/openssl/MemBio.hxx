// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Error.hxx"
#include "UniqueBIO.hxx"
#include "util/AllocatedString.hxx"

#include <span>
#include <string_view>

/**
 * Wrapper for OpenSSL's BIO_new_mem_buf() which takes a std::span and
 * throws on error.
 */
UniqueBIO
BIO_new_mem_buf(std::span<const std::byte> buffer);

[[gnu::pure]]
inline std::string_view
BIO_get_mem_string_view(BIO &bio) noexcept
{
	char *data;
	long length = BIO_get_mem_data(&bio, &data);
	if (length < 0)
		return {};

	return {data, static_cast<std::size_t>(length)};
}

/**
 * Call a function that writes into a memory BIO and return the BIO
 * memory as #AllocatedString instance.
 */
template<typename W>
static inline AllocatedString
BioWriterToString(W &&writer)
{
	UniqueBIO bio(BIO_new(BIO_s_mem()));
	if (bio == nullptr)
		throw SslError("BIO_new() failed");

	writer(*bio);

	return AllocatedString{BIO_get_mem_string_view(*bio)};
}
