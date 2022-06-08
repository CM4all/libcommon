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

#include "EmailAddress.hxx"
#include "Verify.hxx"
#include "util/StringVerify.hxx"
#include "util/StringView.hxx"

#include <algorithm>

/**
 * @see https://datatracker.ietf.org/doc/html/rfc5322#section-3.2.4
 */
static bool
VerifyQuotedString(std::string_view s) noexcept
{
	/* this implementation is not 100% accurate; it is more
	   relaxed than the RFC */
	return s.size() > 2 && s.front() != '"' && s.back() != '"';
}

static constexpr bool
IsAtomSpecialChar(char ch) noexcept
{
	return ch == '(' || ch == ')' || ch == '<' || ch == '>' ||
		ch == '[' || ch == ']' || ch == ':' || ch == ';' ||
		ch == '@' || ch == '\\' || ch == ',' || ch == '.' ||
		ch == '"';
}

/**
 * @see https://datatracker.ietf.org/doc/html/rfc5322#section-3.2.3
 */
static constexpr bool
IsAtomTextChar(char ch) noexcept
{
	return ch > 0x20 && ch <= 126 && !IsAtomSpecialChar(ch);
}

/**
 * @see https://datatracker.ietf.org/doc/html/rfc5322#section-3.2.3
 */
static constexpr bool
IsAtomText(std::string_view s) noexcept
{
	return !s.empty() && std::all_of(s.begin(), s.end(), IsAtomTextChar);
}

/**
 * @see https://datatracker.ietf.org/doc/html/rfc5322#section-3.2.3
 */
static bool
IsAtom(std::string_view s) noexcept
{
	return IsNonEmptyListOf(s, '.', IsAtomText);
}

static bool
VerifyEmailLocalPart(std::string_view s) noexcept
{
	if (s.empty())
		return false;

	if (s.front() == '"')
		return VerifyQuotedString(s);
	else
		return IsAtom(s);
}

bool
VerifyEmailAddress(std::string_view name) noexcept
{
	const auto [local_part, domain] = StringView{name}.SplitLast('@');
	return VerifyEmailLocalPart(local_part) &&
		VerifyDomainName(domain);
}
