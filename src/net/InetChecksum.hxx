// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/ByteOrder.hxx" // for ToBE16()
#include "util/SpanCast.hxx" // for FromBytesFloor()

#include <cstdint>
#include <cstddef> // for std::byte
#include <numeric> // for std::accumulate
#include <span>
#include <type_traits>

/**
 * Calculate an Internet Checksum according to RFC 1071.
 */
class InetChecksum {
	uint_fast32_t sum;

public:
	constexpr InetChecksum(uint16_t init=0) noexcept
		:sum(init) {}

	constexpr auto &Update(std::span<const uint16_t> src) noexcept {
		sum = std::accumulate(src.begin(), src.end(), sum);
		return *this;
	}

	template<typename T>
	requires std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T> && (sizeof(T) % 2 == 0 && alignof(T) % 2 == 0)
	constexpr auto &UpdateT(const T &src) noexcept {
		return Update(std::span{reinterpret_cast<const uint16_t *>(&src), sizeof(src) / 2});
	}

	constexpr auto &UpdateOddByte(std::byte src) noexcept {
		sum += ToBE16(static_cast<uint_fast32_t>(src) << 8);
		return *this;
	}

	constexpr auto &Update(std::span<const std::byte> src) noexcept {
		Update(FromBytesFloor<const uint16_t>(src));
		if (src.size() % 2 != 0)
			UpdateOddByte(src.back());
		return *this;
	}

	constexpr uint16_t Finish() const noexcept {
		uint_fast32_t result = Carry16(Carry16(sum));
		return static_cast<uint16_t>(~result);
	}

private:
	static constexpr uint_fast32_t Carry16(uint_fast32_t x) noexcept {
		return (x >> 16) + (x & 0xffff);
	}
};
