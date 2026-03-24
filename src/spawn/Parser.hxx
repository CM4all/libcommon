// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <algorithm>
#include <span>
#include <string_view>

#include <assert.h>
#include <stdint.h>
#include <string.h>

namespace Spawn {

class MalformedPayloadError {};

class Payload {
	const std::byte *begin, *const end;

public:
	explicit constexpr Payload(std::span<const std::byte> _payload) noexcept
		:begin(_payload.data()), end(begin + _payload.size()) {}

	constexpr bool empty() const noexcept {
		return begin == end;
	}

	constexpr std::size_t GetSize() const noexcept {
		return end - begin;
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
			throw MalformedPayloadError{};

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

	std::string_view ReadStringView() {
		auto n = std::find(begin, end, std::byte{0});
		if (n == end)
			throw MalformedPayloadError{};

		const char *value = (const char *)begin;
		begin = n + 1;
		return {value, reinterpret_cast<const char *>(n)};
	}

	const char *ReadString() {
		/* returning data() is valid here because the
		   std::string_view returned by ReadString() points to
		   a null-terminated string */
		return ReadStringView().data();
	}
};

} // namespace Spawn
