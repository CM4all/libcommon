// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstring>

template<typename B>
std::span<char>
ExtractLine(B &buffer, bool flush=false)
{
	auto r = buffer.Read();
	char *data = reinterpret_cast<char*>(r.data());
	char *newline = reinterpret_cast<char*>(std::memchr(data, '\n', r.size()));
	if (newline == nullptr) {
		if (!r.empty() && (flush || buffer.IsFull())) {
			buffer.Clear();
			return {data, r.size()};
		}

		return {};
	}

	buffer.Consume(newline + 1 - data);

	while (newline > data && newline[-1] == '\r')
		--newline;

	return std::span{data, static_cast<std::size_t>(newline - data)};
}
