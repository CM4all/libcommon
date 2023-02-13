// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json.hpp>

#include <string_view>

namespace Json {

[[gnu::pure]]
static inline std::string_view
GetString(const boost::json::value &json) noexcept
{
	const auto *s = json.if_string();
	return s != nullptr
		? (std::string_view)*s
		: std::string_view{};
}

[[gnu::pure]]
static inline std::string_view
GetString(const boost::json::value *json) noexcept
{
	return json != nullptr
		? GetString(*json)
		: std::string_view{};
}

[[gnu::pure]]
static inline std::string_view
GetString(const boost::json::object &parent, std::string_view key) noexcept
{
	return GetString(parent.if_contains(key));
}

[[gnu::pure]]
static inline const char *
GetCString(const boost::json::value &json) noexcept
{
	const auto *s = json.if_string();
	return s != nullptr
		? s->c_str()
		: nullptr;
}

[[gnu::pure]]
static inline const char *
GetCString(const boost::json::value *json) noexcept
{
	return json != nullptr
		? GetCString(*json)
		: nullptr;
}

[[gnu::pure]]
static inline const char *
GetCString(const boost::json::object &parent, std::string_view key) noexcept
{
	return GetCString(parent.if_contains(key));
}

} // namespace Json
