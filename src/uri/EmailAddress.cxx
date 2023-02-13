// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "EmailAddress.hxx"
#include "Verify.hxx"
#include "util/StringListVerify.hxx"
#include "util/StringSplit.hxx"
#include "util/StringVerify.hxx"

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
	return CheckCharsNonEmpty(s, IsAtomTextChar);
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
	const auto [local_part, domain] = SplitLast(name, '@');
	return VerifyEmailLocalPart(local_part) &&
		VerifyDomainName(domain);
}
