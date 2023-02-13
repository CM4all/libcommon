// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * URI character classification according to RFC 2396.
 */

#pragma once

#include "util/CharUtil.hxx"

/**
 * Is this a "delimiter of the generic URI components"?
 *
 * @see RFC 2396 2.2
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
 * @see RFC 2396 2.2
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
 * @see RFC 2396 2.2
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
 * See RFC 2396 2.3.
 */
constexpr bool
IsUriUnreservedChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

/**
 * @see RFC 2396 2.4.1
 */
constexpr bool
IsUriEscapedChar(char ch) noexcept
{
	return ch == '%' || IsHexDigit(ch);
}

/**
 * @see RFC 2396 2
 */
constexpr bool
IsUricChar(char ch) noexcept
{
	return IsUriReservedChar(ch) || IsUriUnreservedChar(ch) ||
		IsUriEscapedChar(ch);
}

/**
 * See RFC 2396 3.3.
 */
constexpr bool
IsUriPchar(char ch) noexcept
{
	return IsUriUnreservedChar(ch) ||
		ch == '%' || /* "escaped" */
		ch == ':' || ch == '@' || ch == '&' || ch == '=' || ch == '+' ||
		ch == '$' || ch == ',';
}
