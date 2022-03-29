/*
 * Copyright 2007-2022 CM4all GmbH
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

/**
 * Allocate pages from the kernel
 *
 * Throws std::bad_alloc on error.
 *
 * @param size the size of the allocation; must be a multiple of
 * #PAGE_SIZE
 */
void *
AllocatePages(std::size_t size);

static inline void
FreePages(void *p, std::size_t size) noexcept
{
	munmap(p, size);
}

/**
 * Allow the Linux kernel to use "Huge Pages" for the cache, which
 * reduces page table overhead for this big chunk of data.
 *
 * @param size a multiple of #HUGE_PAGE_SIZE
 */
static inline void
EnableHugePages(void *p, std::size_t size) noexcept
{
#ifdef MADV_HUGEPAGE
	madvise(p, size, MADV_HUGEPAGE);
#else
	(void)p;
	(void)size;
#endif
}

/**
 * Controls whether forked processes inherit the specified pages.
 */
static inline void
EnablePageFork(void *p, std::size_t size, bool inherit) noexcept
{
#ifdef __linux__
	madvise(p, size, inherit ? MADV_DOFORK : MADV_DONTFORK);
#else
	(void)p;
	(void)size;
	(void)inherit;
#endif
}

/**
 * Discard the specified page contents, giving memory back to the
 * kernel.  The mapping is preserved, and new memory will be allocated
 * automatically on the next write access.
 */
static inline void
DiscardPages(void *p, std::size_t size) noexcept
{
#ifdef __linux__
	madvise(p, size, MADV_DONTNEED);
#else
	(void)p;
	(void)size;
#endif
}
