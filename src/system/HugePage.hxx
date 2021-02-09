/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

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
	return ((size - 1) | (HUGE_PAGE_SIZE - 1)) + 1;
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
	return size & ~(HUGE_PAGE_SIZE - 1);
}

static_assert(AlignHugePageDown(0) == 0, "Rounding bug");
static_assert(AlignHugePageDown(1) == 0, "Rounding bug");
static_assert(AlignHugePageDown(HUGE_PAGE_SIZE - 1) == 0, "Rounding bug");
static_assert(AlignHugePageDown(HUGE_PAGE_SIZE) == HUGE_PAGE_SIZE, "Rounding bug");
