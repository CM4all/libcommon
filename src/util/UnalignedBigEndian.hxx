// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

constexpr std::byte *
WriteU8(std::byte *p, uint_least8_t value) noexcept
{
	*p++ = static_cast<std::byte>(value);
	return p;
}

constexpr std::byte *
WriteUnalignedBE16(std::byte *p, uint_least16_t value) noexcept
{
	p = WriteU8(p, value >> 8);
	return WriteU8(p, value);
}

constexpr std::byte *
WriteUnalignedBE32(std::byte *p, uint_least32_t value) noexcept
{
	p = WriteU8(p, value >> 24);
	p = WriteU8(p, value >> 16);
	p = WriteU8(p, value >> 8);
	return WriteU8(p, value);
}

constexpr std::byte *
WriteUnalignedBE64(std::byte *p, uint_least64_t value) noexcept
{
	p = WriteU8(p, value >> 56);
	p = WriteU8(p, value >> 48);
	p = WriteU8(p, value >> 40);
	p = WriteU8(p, value >> 32);
	p = WriteU8(p, value >> 24);
	p = WriteU8(p, value >> 16);
	p = WriteU8(p, value >> 8);
	return WriteU8(p, value);
}

constexpr uint_least16_t
ReadUnalignedBE16(std::span<const std::byte, 2> src) noexcept
{
	return (static_cast<uint_least16_t>(src[0]) << 8) |
		static_cast<uint_least16_t>(src[1]);
}

constexpr uint_least32_t
ReadUnalignedBE32(std::span<const std::byte, 4> src) noexcept
{
	return (static_cast<uint_least32_t>(src[0]) << 24) |
		(static_cast<uint_least32_t>(src[1]) << 16) |
		(static_cast<uint_least32_t>(src[2]) << 8) |
		static_cast<uint_least32_t>(src[3]);
}

constexpr uint_least64_t
ReadUnalignedBE64(std::span<const std::byte, 8> src) noexcept
{
	return (static_cast<uint_least64_t>(src[0]) << 56) |
		(static_cast<uint_least64_t>(src[1]) << 48) |
		(static_cast<uint_least64_t>(src[2]) << 40) |
		(static_cast<uint_least64_t>(src[3]) << 32) |
		(static_cast<uint_least64_t>(src[4]) << 24) |
		(static_cast<uint_least64_t>(src[5]) << 16) |
		(static_cast<uint_least64_t>(src[6]) << 8) |
		static_cast<uint_least64_t>(src[7]);
}
