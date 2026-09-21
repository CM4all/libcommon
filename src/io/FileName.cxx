// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "FileName.hxx"
#include "util/StringCompare.hxx" // for StringIsEmpty()

#include <cassert>
#include <cstring> // for strchr()

bool
IsValidFilename(const char *s) noexcept
{
	assert(s != nullptr);

	return !StringIsEmpty(s) && !IsSpecialFilename(s) && strchr(s, '/') == 0;
}
