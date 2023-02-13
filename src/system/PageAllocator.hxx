// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
 * Controls whether the specified pages will be included in a core
 * dump.
 */
static inline void
EnablePageDump(void *p, std::size_t size, bool dump) noexcept
{
#ifdef __linux__
	madvise(p, size, dump ? MADV_DODUMP : MADV_DONTDUMP);
#else
	(void)p;
	(void)size;
	(void)dump;
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
