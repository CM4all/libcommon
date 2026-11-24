// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Error.hxx"
#include "util/UnalignedBigEndian.hxx" // for ReadUnalignedBE*()

#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

namespace Net::Log {

class Deserializer {
	const std::byte *p;
	const std::byte *end;

public:
	Deserializer(std::span<const std::byte> src) noexcept
		:p(src.data()), end(p + src.size()) {
		assert(p != nullptr);
		assert(end != nullptr);
		assert(p <= end);
	}

	bool empty() const noexcept {
		return p == end;
	}

	template<std::size_t size>
	requires(size != std::dynamic_extent)
	std::span<const std::byte, size> ReadSpan() {
		return std::span<const std::byte, size>{ReadRaw(size), size};
	}

	std::byte ReadByte() {
		return ReadT<std::byte>();
	}

	uint16_t ReadU16() {
		return ReadUnalignedBE16(ReadSpan<2>());
	}

	uint64_t ReadU64() {
		return ReadUnalignedBE64(ReadSpan<8>());
	}

	std::string_view ReadStringView() {
		const char *result = (const char *)p;
		const std::size_t remaining = (const char *)end - result;
		const size_t length = strnlen(result, remaining);
		if (length >= remaining)
			/* null terminator not found */
			throw ProtocolError();

		const std::byte *after = (const std::byte *)(result + length + 1);
		p = after;
		return {result, length};
	}

	const char *ReadString() {
		return ReadStringView().data();
	}

private:
	const std::byte *PeekRaw(std::size_t size) {
		const std::byte *result = p;
		if (result + size > end)
			throw ProtocolError{};

		return result;
	}

	const std::byte *ReadRaw(std::size_t size) {
		const std::byte *result = PeekRaw(size);
		p += size;
		return result;
	}

	template<typename T>
	requires(alignof(T) == 1)
	const T &ReadT() {
		return *reinterpret_cast<const T *>(ReadRaw(sizeof(T)));
	}
};

} // namespace Net::Log
