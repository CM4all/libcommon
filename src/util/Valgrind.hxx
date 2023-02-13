// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#ifdef HAVE_VALGRIND_MEMCHECK_H

#include <valgrind/memcheck.h>

[[gnu::const]]
static inline bool
HaveValgrind() noexcept
{
	return RUNNING_ON_VALGRIND;
}

#else

constexpr bool
HaveValgrind() noexcept
{
	return false;
}

#endif
