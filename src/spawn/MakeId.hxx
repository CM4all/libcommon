// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/Base32.hxx"
#include "util/djb_hash.hxx"

#include <algorithm> // for std::copy()
#include <cstring> // for stpcpy()
#include <span>
#include <string_view>

[[nodiscard]]
inline char *
AppendString(char *p, std::string_view s) noexcept
{
	return std::copy(s.begin(), s.end(), p);
}

[[nodiscard]]
inline char *
AppendString(char *p, const char *s) noexcept
{
	return stpcpy(p, s);
}

[[nodiscard]]
inline char *
AppendOptional(char *p, std::string_view s, bool append) noexcept
{
	if (append)
		p = AppendString(p, s);

	return p;
}

[[nodiscard]]
inline char *
AppendOptional(char *p, char ch, bool append) noexcept
{
	if (append)
		*p++ = ch;

	return p;
}

[[nodiscard]]
inline char *
AppendValue(char *p, std::string_view prefix, const char *value) noexcept
{
	p = AppendString(p, prefix);
	p = AppendString(p, value);
	return p;
}

[[nodiscard]]
inline char *
AppendOptionalValue(char *p, std::string_view prefix, const char *value) noexcept
{
	if (value != nullptr)
		p = AppendValue(p, prefix, value);

	return p;
}

template<std::integral I>
[[nodiscard]]
inline char *
AppendIntBase32(char *p, std::string_view prefix, I value) noexcept
{
	p = AppendString(p, prefix);
	p = FormatIntBase32(p, value);
	return p;
}

[[nodiscard]]
inline char *
AppendDjbHash(char *p, std::string_view prefix, const char *value) noexcept
{
	return AppendIntBase32(p, prefix, djb_hash_string(value));
}

[[nodiscard]]
inline char *
AppendDjbHash(char *p, std::string_view prefix,
	      std::span<const std::byte> value) noexcept
{
	return AppendIntBase32(p, prefix, djb_hash(value));
}

[[nodiscard]]
inline char *
AppendOptionalDjbHash(char *p, std::string_view prefix, const char *value) noexcept
{
	if (value != nullptr)
		p = AppendDjbHash(p, prefix, value);

	return p;
}

[[nodiscard]]
inline char *
AppendOptionalDjbHash(char *p, std::string_view prefix,
		      std::span<const std::byte> value) noexcept
{
	if (value.data() != nullptr)
		p = AppendDjbHash(p, prefix, value);

	return p;
}
