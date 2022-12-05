/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
	return ch == ':' || ch == '/' || ch == '?' || ch == '@';
}

/**
 * Is this a "subcomponent delimiter"?
 *
 * @see RFC 2396 2.2
 */
constexpr bool
IsUriSubcomponentDelimiter(char ch) noexcept
{
	return ch == '$' || ch == '&' ||
		ch == '+' || ch == ',' ||
		ch == ';' || ch == '=';
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
