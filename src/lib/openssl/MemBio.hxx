// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#ifndef SSL_MEM_BIO_HXX
#define SSL_MEM_BIO_HXX

#include "Error.hxx"
#include "UniqueBIO.hxx"
#include "util/AllocatedString.hxx"

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

	char *data;
	long length = BIO_get_mem_data(bio.get(), &data);
	const std::string_view src(data, length);
	return AllocatedString(src);
}

#endif
