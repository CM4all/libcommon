// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string.h>

template<typename B>
auto
ExtractLine(B &buffer, bool flush=false)
{
	auto r = buffer.Read();
	char *newline = (char *)memchr(r.data(), '\n', r.size());
	if (newline == nullptr) {
		if (!r.empty() && (flush || buffer.IsFull())) {
			buffer.Clear();
			return r;
		}

		return decltype(r){};
	}

	buffer.Consume(newline + 1 - r.data());

	while (newline > r.data() && newline[-1] == '\r')
		--newline;

	return r.subspan(0, newline - r.data());
}
