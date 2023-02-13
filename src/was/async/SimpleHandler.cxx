// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SimpleHandler.hxx"
#include "util/MimeType.hxx"

using std::string_view_literals::operator""sv;

namespace Was {

bool
SimpleRequest::IsContentType(const std::string_view expected) const noexcept
{
	auto i = headers.find("content-type"sv);
	return i != headers.end() &&
		GetMimeTypeBase(i->second) == expected;
}

} // namespace Was
