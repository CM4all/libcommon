// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>

namespace BengProxy {

constexpr bool
IsControlSizePadded(std::size_t size) noexcept
{
	return size % 4 == 0;
}

constexpr std::size_t
ControlPaddingSize(std::size_t size) noexcept
{
	return (3 - ((size - 1) & 0x3));
}

static_assert(ControlPaddingSize(0) == 0);
static_assert(ControlPaddingSize(1) == 3);
static_assert(ControlPaddingSize(2) == 2);
static_assert(ControlPaddingSize(3) == 1);
static_assert(ControlPaddingSize(4) == 0);
static_assert(ControlPaddingSize(5) == 3);

constexpr std::size_t
PadControlSize(std::size_t size) noexcept
{
	return ((size + 3) | 3) - 3;
}

static_assert(PadControlSize(0) == 0);
static_assert(PadControlSize(1) == 4);
static_assert(PadControlSize(2) == 4);
static_assert(PadControlSize(3) == 4);
static_assert(PadControlSize(4) == 4);
static_assert(PadControlSize(5) == 8);

} // namespace BengProxy
