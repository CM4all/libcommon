// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SimpleResponse.hxx"
#include "SimpleOutput.hxx"
#include "util/DisposableBuffer.hxx"

namespace Was {

void
SimpleResponse::SetTextPlain(std::string_view _body) noexcept
{
	body = std::make_unique<SimpleOutput>(DisposableBuffer{ToNopPointer(_body.data()), _body.size()});
	headers.emplace("content-type", "text/plain");
}

} // namespace Was
