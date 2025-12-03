// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SimpleResponse.hxx"
#include "SpanOutputProducer.hxx"
#include "StringOutputProducer.hxx"
#include "util/SpanCast.hxx"

using std::string_view_literals::operator""sv;

namespace Was {

void
SimpleResponse::SetTextPlain(std::string_view _body) noexcept
{
	body = std::make_unique<SpanOutputProducer>(AsBytes(_body));
	headers.emplace("content-type"sv, "text/plain"sv);
}

void
SimpleResponse::MoveTextPlain(std::string &&_body) noexcept
{
	body = std::make_unique<StringOutputProducer>(std::move(_body));
	headers.emplace("content-type"sv, "text/plain"sv);
}

void
SimpleResponse::CopyTextPlain(std::string_view _body) noexcept
{
	MoveTextPlain(std::string{_body});
}

} // namespace Was
