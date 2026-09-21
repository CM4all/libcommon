// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

/**
 * Is this a "special" filename, i.e. a filename with a special
 * meaning defined by the operating system.  These files should
 * usually be ignored in directory listings and such file names cannot
 * be created manually.
 */
[[gnu::pure]]
constexpr bool
IsSpecialFilename(const char *s) noexcept
{
	return s[0] == '.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0));
}

/**
 * Is this a valid filename (that can be created)?  It must be
 * non-empty, not be a "special" filename as defined by
 * IsSpecialFilename() and must not contain a slash.
 */
[[gnu::pure]]
bool
IsValidFilename(const char *s) noexcept;
