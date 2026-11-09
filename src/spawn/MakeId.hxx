// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/Base32.hxx"
#include "util/djb_hash.hxx"
#include "util/StringBuilder.hxx"

#include <fmt/core.h>

#include <span>
#include <string_view>

inline void
AppendString(StringBuilder &b, std::string_view s)
{
	b.Append(s);
}

inline void
AppendString(StringBuilder &b, const char *s)
{
	b.Append(std::string_view{s});
}

inline void
AppendOptional(StringBuilder &b, std::string_view s, bool append)
{
	if (append)
		AppendString(b, s);
}

inline void
AppendOptional(StringBuilder &b, char ch, bool append)
{
	if (append)
		b.Append(ch);
}

inline void
AppendValue(StringBuilder &b, std::string_view prefix, std::string_view value)
{
	AppendString(b, prefix);
	AppendString(b, value);
}

inline void
AppendValue(StringBuilder &b, std::string_view prefix, const char *value)
{
	AppendString(b, prefix);
	AppendString(b, value);
}

inline void
AppendOptionalValue(StringBuilder &b, std::string_view prefix, const char *value)
{
	if (value != nullptr)
		AppendValue(b, prefix, value);
}

template<std::integral I>
inline void
AppendIntBase32(StringBuilder &b, I value)
{
	b.CheckAppend(MAX_INT_BASE32_LENGTH<I>);

	const auto w = b.Write();
	char *p = FormatIntBase32(w.data(), value);
	b.Extend(p - w.data());
}

template<std::integral I>
inline void
AppendIntBase32(StringBuilder &b, std::string_view prefix, I value)
{
	b.Append(prefix);
	AppendIntBase32(b, value);
}

inline void
AppendDjbHash(StringBuilder &b, std::string_view prefix, const char *value)
{
	AppendIntBase32(b, prefix, djb_hash_string(value));
}

inline void
AppendDjbHash(StringBuilder &b, std::string_view prefix,
	      std::span<const std::byte> value)
{
	AppendIntBase32(b, prefix, djb_hash(value));
}

inline void
AppendOptionalDjbHash(StringBuilder &b, std::string_view prefix, const char *value)
{
	if (value != nullptr)
		AppendDjbHash(b, prefix, value);
}

inline void
AppendOptionalDjbHash(StringBuilder &b, std::string_view prefix,
		      std::span<const std::byte> value)
{
	if (value.data() != nullptr)
		AppendDjbHash(b, prefix, value);
}

constexpr void
AppendVFmt(StringBuilder &b,
	   fmt::string_view format_str, fmt::format_args args)
{
	const auto w = b.Write();
	const auto r = fmt::vformat_to_n(w.begin(), w.size(), format_str, args);
	if (r.size > w.size())
		throw TooLargeError{};

	b.Extend(r.size);
}

template<typename S, typename... Args>
constexpr void
AppendFmt(StringBuilder &b, const S &format_str, Args&&... args)
{
	AppendVFmt(b, format_str, fmt::make_format_args(args...));
}
