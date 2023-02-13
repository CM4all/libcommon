// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json.hpp>

#include <cstddef>
#include <string_view>

namespace Json {

static inline auto &
Make(boost::json::object &o, std::string_view key) noexcept
{
	return o[key];
}

static inline auto &
Make(boost::json::value &v, std::string_view key) noexcept
{
	if (!v.is_object())
		v.emplace_object();

	return Make(v.get_object(), key);
}

static inline auto &
Make(boost::json::array &a, std::size_t i) noexcept
{
	if (a.size() <= i)
		a.resize(i + 1);

	return a[i];
}

static inline auto &
Make(boost::json::value &v, std::size_t i) noexcept
{
	if (!v.is_array())
		v.emplace_array();

	return Make(v.get_array(), i);
}

template<typename J, typename K, typename... Args>
static inline auto &
Make(J &j, K &&key, Args&&... args)
{
	return Make(Make(j, std::forward<K>(key)),
		    std::forward<Args>(args)...);
}

} // namespace Json
