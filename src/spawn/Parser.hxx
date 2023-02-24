// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
	explicit constexpr SpawnPayload(std::span<const std::byte> _payload) noexcept
		:begin(_payload.data()), end(begin + _payload.size()) {}

	constexpr bool empty() const noexcept {
		return begin == end;
	}

	constexpr std::size_t GetSize() const noexcept {
		return begin - end;
	}

	std::byte ReadByte() noexcept {
		assert(!empty());
		return *begin++;
	}

	bool ReadBool() noexcept {
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

	void ReadUnsigned(unsigned &value_r) {
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
