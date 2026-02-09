// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/RoundPowerOfTwo.hxx"

#include <cstddef>

#include <sys/mman.h>

static constexpr std::size_t PAGE_SIZE = 4096;

/**
 * Round up the parameter, make it page-aligned.
 */
static constexpr std::size_t
AlignToPageSize(std::size_t size) noexcept
{
	return RoundUpToPowerOfTwo(size, PAGE_SIZE);
}
