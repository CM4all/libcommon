// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PageAllocator.hxx"

#include <new>

void *
AllocatePages(std::size_t size)
{
	void *p = mmap(NULL, size, PROT_READ|PROT_WRITE,
		       MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	if (p == MAP_FAILED)
		throw std::bad_alloc{};

	return p;
}
