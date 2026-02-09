// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "LargeAllocation.hxx"
#include "PageAllocator.hxx"
#include "PageSize.hxx"

#include <new>

#include <sys/mman.h>
#include <unistd.h>

LargeAllocation::LargeAllocation(size_t _size)
{
	_size = AlignToPageSize(_size);
	ptr = {AllocatePages(_size), _size};
}

void
LargeAllocation::Free(std::span<std::byte> p) noexcept
{
	FreePages(p);
}
