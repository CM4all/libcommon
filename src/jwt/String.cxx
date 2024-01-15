// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "String.hxx"
#include "util/CharUtil.hxx"
#include "util/StringSplit.hxx"
#include "util/StringVerify.hxx"

static constexpr bool
IsBase64UrlChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '_' || ch == '-';
}

static constexpr bool
IsBase64UrlSyntax(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsBase64UrlChar);
}

namespace JWT {

bool
CheckSyntax(std::string_view jwt) noexcept
{
	const auto [header, payload_and_signature] = Split(jwt, '.');
	const auto [payload, signature_and_rest] = Split(payload_and_signature, '.');
	const auto [signature, rest] = Split(signature_and_rest, '.');

	return IsBase64UrlSyntax(header) && IsBase64UrlSyntax(payload) &&
		IsBase64UrlSyntax(signature) && rest.data() == nullptr;
}

} // namespace JWT
