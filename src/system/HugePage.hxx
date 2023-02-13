// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/RoundPowerOfTwo.hxx"

#include <cstddef>

#ifndef __linux__
#error This header is Linux-specific.
#endif

// TODO: architectures other than x86_64 may have other values
static constexpr std::size_t HUGE_PAGE_SIZE = 512 * 4096;

/**
 * Align the given size to the next huge page size, rounding up.
 */
constexpr std::size_t
AlignHugePageUp(std::size_t size) noexcept
{
	return RoundUpToPowerOfTwo(size, HUGE_PAGE_SIZE);
}

static_assert(AlignHugePageUp(0) == 0, "Rounding bug");
static_assert(AlignHugePageUp(1) == HUGE_PAGE_SIZE, "Rounding bug");
static_assert(AlignHugePageUp(HUGE_PAGE_SIZE - 1) == HUGE_PAGE_SIZE, "Rounding bug");
static_assert(AlignHugePageUp(HUGE_PAGE_SIZE) == HUGE_PAGE_SIZE, "Rounding bug");

/**
 * Align the given size to the next huge page size, rounding down.
 */
constexpr std::size_t
AlignHugePageDown(std::size_t size) noexcept
{
	return RoundDownToPowerOfTwo(size, HUGE_PAGE_SIZE);
}

static_assert(AlignHugePageDown(0) == 0, "Rounding bug");
static_assert(AlignHugePageDown(1) == 0, "Rounding bug");
static_assert(AlignHugePageDown(HUGE_PAGE_SIZE - 1) == 0, "Rounding bug");
static_assert(AlignHugePageDown(HUGE_PAGE_SIZE) == HUGE_PAGE_SIZE, "Rounding bug");
