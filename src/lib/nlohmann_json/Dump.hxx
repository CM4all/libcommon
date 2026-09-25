// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace Json {

/**
 * Serialize a JSON value without throwing.  Invalid UTF-8 sequences
 * are silently replaced with U+FFFD.
 *
 * Use this function instead of nlohmann::json::dump()'s defaults if
 * you do not want to handle UTF-8 validation errors because you do
 * not care enough.
 */
[[nodiscard]] [[gnu::pure]]
inline std::string
DumpSloppy(const nlohmann::json &j) noexcept
{
	return j.dump(-1, ' ', false,
		      nlohmann::json::error_handler_t::replace);
}

} // namespace Json
