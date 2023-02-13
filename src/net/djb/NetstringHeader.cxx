// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "NetstringHeader.hxx"

#include <stdio.h>

std::string_view
NetstringHeader::operator()(std::size_t size) noexcept
{
	return {buffer, (std::size_t)sprintf(buffer, "%zu:", size)};
}
