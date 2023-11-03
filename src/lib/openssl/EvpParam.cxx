// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "EvpParam.hxx"

std::unique_ptr<char[]>
GetStringParam(const EVP_PKEY &key, const char *name)
{
	std::size_t length;
	if (!EVP_PKEY_get_utf8_string_param(&key, name, nullptr, 0, &length))
		throw SslError{};

	auto result = std::make_unique<char[]>(length + 1);

	if (!EVP_PKEY_get_utf8_string_param(&key, name, result.get(), length + 1, &length))
		throw SslError{};

	return result;
}
