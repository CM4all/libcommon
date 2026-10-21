// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "FileName.hxx"
#include "util/StringCompare.hxx" // for StringIsEmpty()

#include <cassert>
#include <cstring> // for strchr()

using std::string_view_literals::operator""sv;

bool
IsValidFilename(const char *s) noexcept
{
	assert(s != nullptr);

	return !StringIsEmpty(s) && !IsSpecialFilename(s) && strchr(s, '/') == 0;
}

bool
IsValidFilename(std::string_view s) noexcept
{
	return !s.empty() && !IsSpecialFilename(s) &&
		s.find_first_of("/\0"sv) == s.npos;
}
