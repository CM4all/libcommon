// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "util/Sanitizer.hxx"
#include "util/Valgrind.hxx"

[[gnu::const]]
static inline bool
HaveMemoryChecker() noexcept
{
	if (HaveAddressSanitizer())
		return true;

	if (HaveValgrind())
		return true;

	return false;
}
