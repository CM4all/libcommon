// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LargeAllocation.hxx"
#include "PageAllocator.hxx"

#include <new>

#include <sys/mman.h>
#include <unistd.h>

LargeAllocation::LargeAllocation(size_t _size)
	:the_size(AlignToPageSize(_size))
{
	data = AllocatePages(the_size);
}

void
LargeAllocation::Free(void *p, size_t size) noexcept
{
	FreePages(p, size);
}
