// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>

namespace BengControl {

constexpr bool
IsSizePadded(std::size_t size) noexcept
{
	return size % 4 == 0;
}

constexpr std::size_t
PaddingSize(std::size_t size) noexcept
{
	return (3 - ((size - 1) & 0x3));
}

static_assert(PaddingSize(0) == 0);
static_assert(PaddingSize(1) == 3);
static_assert(PaddingSize(2) == 2);
static_assert(PaddingSize(3) == 1);
static_assert(PaddingSize(4) == 0);
static_assert(PaddingSize(5) == 3);

constexpr std::size_t
PadSize(std::size_t size) noexcept
{
	return ((size + 3) | 3) - 3;
}

static_assert(PadSize(0) == 0);
static_assert(PadSize(1) == 4);
static_assert(PadSize(2) == 4);
static_assert(PadSize(3) == 4);
static_assert(PadSize(4) == 4);
static_assert(PadSize(5) == 8);

} // namespace BengControl
