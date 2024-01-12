// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ToDisposableBuffer.hxx"
#include "util/DisposableBuffer.hxx"

#include <nlohmann/json.hpp>

namespace Json {

static DisposableBuffer
ToDisposableBuffer(std::string_view src) noexcept
{
	char *dest = new char[src.size()];
	std::copy(src.begin(), src.end(), dest);
	return {ToDeleteArray(dest), src.size()};
}

DisposableBuffer
ToDisposableBuffer(const nlohmann::json &j) noexcept
{
	return ToDisposableBuffer(static_cast<std::string_view>(j.dump()));
}

} // namespace Json
