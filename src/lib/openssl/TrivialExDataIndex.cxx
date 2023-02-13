// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "TrivialExDataIndex.hxx"
#include "Error.hxx"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace OpenSSL {

TrivialExDataIndex::TrivialExDataIndex()
	:idx(SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr))
{
	if (idx < 0)
		throw SslError("SSL_get_ex_new_index() failed");
}

void
TrivialExDataIndex::Set(SSL &ssl, void *value) const noexcept
{
	SSL_set_ex_data(&ssl, idx, value);
}

void *
TrivialExDataIndex::Get(SSL &ssl) const noexcept
{
	return SSL_get_ex_data(&ssl, idx);
}

} // namespace OpenSSL
