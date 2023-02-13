// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string.h>

/**
 * A function object which compares two strings.  It can be used as a
 * compare type for std::map.
 */
struct StringLess {
	[[gnu::pure]]
	bool operator()(const char *a, const char *b) const noexcept {
		return strcmp(a, b) < 0;
	}
};
