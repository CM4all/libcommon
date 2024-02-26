// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>

/**
 * Parse a bool represented by either "yes" or "no"; throws
 * std::runtime_error on error.
 */
bool
ParseBool(const char *s);

unsigned long
ParseUnsignedLong(const char *s);

unsigned long
ParsePositiveLong(const char *s);

unsigned long
ParsePositiveLong(const char *s, unsigned long max_value);

/**
 * Parse a string as a byte size.
 *
 * Throws on error.
 */
std::size_t
ParseSize(const char *s);
