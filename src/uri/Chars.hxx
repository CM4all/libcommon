// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * URI character classification according to RFC 3986.
 */

#pragma once

#include "util/CharUtil.hxx"

/**
 * Is this a "delimiter of the generic URI components"?
 *
 * @see RFC 3986 2.2, "gen-delims"
 */
constexpr bool
IsUriGenericDelimiter(char ch) noexcept
{
	return ch == ':' || ch == '/' || ch == '?' || ch == '#' ||
		ch == '[' || ch == ']' ||ch == '@';
}

/**
 * Is this a "subcomponent delimiter"?
 *
 * @see RFC 3986 2.2, "sub-delims"
 */
constexpr bool
IsUriSubcomponentDelimiter(char ch) noexcept
{
	return ch == '!' || ch == '$' || ch == '&' || ch == '\'' ||
		ch == '(' || ch == ')' ||
		ch == '*' || ch == '+' ||
		ch == ',' || ch == ';' || ch == '=';
}

/**
 * Is this a "reserved character"?
 *
 * @see RFC 3986 2.2, "reserved"
 */
constexpr bool
IsUriReservedChar(char ch) noexcept
{
	return IsUriGenericDelimiter(ch) || IsUriSubcomponentDelimiter(ch);
}

/**
 * "Characters that are allowed in a URI but do not have a reserved
 * purpose are called unreserved."
 *
 * See RFC 3986 2.3, "unreserved"
 */
constexpr bool
IsUriUnreservedChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

/**
 * @see RFC 3986 2.1, "escaped" and "pct-encoded"
 */
constexpr bool
IsUriEscapedChar(char ch) noexcept
{
	return ch == '%' || IsHexDigit(ch);
}

/**
 * See RFC 3986 3.3, "pchar"
 */
constexpr bool
IsUriPchar(char ch) noexcept
{
	return IsUriUnreservedChar(ch) ||
		IsUriEscapedChar(ch) ||
		IsUriSubcomponentDelimiter(ch) ||
		ch == ':' || ch == '@';
}

/**
 * @see RFC 2396 3.4, "query"
 */
constexpr bool
IsUriQueryChar(char ch) noexcept
{
	return IsUriPchar(ch) || ch == '/' || ch == '?';
}
