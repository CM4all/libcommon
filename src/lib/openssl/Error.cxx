// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Error.hxx"
#include "MemBio.hxx"

#include <openssl/err.h>

static AllocatedString
ErrToString(std::string_view prefix) noexcept
{
	return BioWriterToString([prefix](BIO &bio){
		if (!prefix.empty()) {
			BIO_write(&bio, prefix.data(), prefix.size());
			BIO_write(&bio, ": ", 2);
		}

		ERR_print_errors(&bio);
	});
}

SslError::SslError(std::string_view msg) noexcept
	:std::runtime_error(ErrToString(msg).c_str()) {}
