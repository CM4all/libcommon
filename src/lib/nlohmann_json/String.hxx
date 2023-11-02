// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <string_view>

namespace Json {

[[gnu::pure]]
inline std::string_view
GetStringRobust(const nlohmann::json &j, std::string_view key) noexcept
{
	if (const auto i = j.find(key); i != j.end() && i->is_string())
		return i->get<std::string_view>();
	else
		return {};
}

} // namespace Json
