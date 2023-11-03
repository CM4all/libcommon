// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UniqueBN.hxx"
#include "Error.hxx"

#include <openssl/evp.h>

std::unique_ptr<char[]>
GetStringParam(const EVP_PKEY &key, const char *name);

template<bool clear>
inline UniqueBIGNUM<clear>
GetBNParam(const EVP_PKEY &key, const char *name)
{
	BIGNUM *result = nullptr;
	if (!EVP_PKEY_get_bn_param(&key, name, &result))
		throw SslError{};

	return UniqueBIGNUM<clear>{result};
}
