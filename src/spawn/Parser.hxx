/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <algorithm>
#include <span>

#include <assert.h>
#include <stdint.h>
#include <string.h>

class MalformedSpawnPayloadError {};

class SpawnPayload {
	const std::byte *begin, *const end;

public:
	explicit SpawnPayload(std::span<const std::byte> _payload)
		:begin(_payload.data()), end(begin + _payload.size()) {}

	bool IsEmpty() const {
		return begin == end;
	}

	size_t GetSize() const {
		return begin - end;
	}

	std::byte ReadByte() {
		assert(!IsEmpty());
		return *begin++;
	}

	bool ReadBool() {
		return (bool)ReadByte();
	}

	void Read(void *p, size_t size) {
		if (GetSize() < size)
			throw MalformedSpawnPayloadError();

		memcpy(p, begin, size);
		begin += size;
	}

	template<typename T>
	void ReadT(T &value_r) {
		Read(&value_r, sizeof(value_r));
	}

	void ReadInt(int &value_r) {
		ReadT(value_r);
	}

	const char *ReadString() {
		auto n = std::find(begin, end, std::byte{0});
		if (n == end)
			throw MalformedSpawnPayloadError();

		const char *value = (const char *)begin;
		begin = n + 1;
		return value;
	}
};
