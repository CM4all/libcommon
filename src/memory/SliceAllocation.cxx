// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SliceAllocation.hxx"
#include "SliceArea.hxx"
#include "Checker.hxx"

#include <cstdlib> // for free()

void
SliceAllocation::Free() noexcept
{
	assert(IsDefined());

	if (HaveMemoryChecker())
		free(data);
	else
		area->Free(data);

	data = nullptr;
}
