// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @see http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64BitsDiv
 */
constexpr std::byte
BitReverseMultiplyModulus(std::byte _in) noexcept
{
	uint64_t in = static_cast<uint64_t>(_in);
	return static_cast<std::byte>((in * 0x0202020202ULL & 0x010884422010ULL) % 1023);
}

static_assert(std::byte{} == BitReverseMultiplyModulus(std::byte{}));
static_assert(std::byte{0xff} == BitReverseMultiplyModulus(std::byte{0xff}));
static_assert(std::byte{0x3c} == BitReverseMultiplyModulus(std::byte{0x3c}));
static_assert(std::byte{0x3c} == BitReverseMultiplyModulus(std::byte{0x3c}));
static_assert(std::byte{0x80} == BitReverseMultiplyModulus(std::byte{0x01}));
static_assert(std::byte{0xc0} == BitReverseMultiplyModulus(std::byte{0x03}));
static_assert(std::byte{0xf0} == BitReverseMultiplyModulus(std::byte{0x0f}));
static_assert(std::byte{0x01} == BitReverseMultiplyModulus(std::byte{0x80}));
static_assert(std::byte{0x03} == BitReverseMultiplyModulus(std::byte{0xc0}));
static_assert(std::byte{0x0f} == BitReverseMultiplyModulus(std::byte{0xf0}));
