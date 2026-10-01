// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "util/CharUtil.hxx"
#include "util/SpanCast.hxx"
#include "util/StringVerify.hxx"

#include <algorithm> // for std::find()

namespace Translation {

[[gnu::pure]]
static constexpr bool
HasNullByte(std::span<const std::byte> p) noexcept
{
	return std::find(p.begin(), p.end(), std::byte{0}) != p.end();
}

[[gnu::pure]]
static constexpr bool
IsValidString(std::string_view s) noexcept
{
	return !HasNullByte(AsBytes(s));
}

[[gnu::pure]]
static constexpr bool
IsValidNonEmptyString(std::string_view s) noexcept
{
	return !s.empty() && IsValidString(s);
}

static constexpr bool
IsValidNameChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '-' || ch == '_';
}

static constexpr bool
IsValidName(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidNameChar);
}

[[gnu::pure]]
static constexpr bool
IsValidAbsolutePath(std::string_view p) noexcept
{
	return IsValidNonEmptyString(p) && p.front() == '/';
}

[[gnu::pure]]
static constexpr bool
IsValidAbsoluteUriPath(std::string_view p) noexcept
{
	return IsValidAbsolutePath(p);
}

/**
 * Is this a valid cookie value character according to RFC 6265 4.1.1?
 */
static constexpr bool
IsValidCookieValueChar(char ch) noexcept
{
	return IsASCII(ch) && !IsWhitespaceFast(ch) && ch != 0x7f &&
		ch != '"' && ch != ',' && ch != ';' && ch != '\\';
}

static constexpr bool
IsValidCookieValue(std::string_view s) noexcept
{
	return CheckChars(s, IsValidCookieValueChar);
}

static constexpr bool
IsValidLowerHeaderNameChar(char ch) noexcept
{
	return IsLowerAlphaASCII(ch) || IsDigitASCII(ch) || ch == '-';
}

static constexpr bool
IsValidLowerHeaderName(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidLowerHeaderNameChar);
}

} // namespace Translation
