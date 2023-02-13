// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json.hpp>

#include <cstddef>
#include <string_view>

namespace Json {

[[gnu::pure]]
static inline const auto *
Lookup(const boost::json::object &o, std::string_view key) noexcept
{
	return o.if_contains(key);
}

[[gnu::pure]]
static inline const auto *
Lookup(const boost::json::value &v, std::string_view key) noexcept
{
	const auto *o = v.if_object();
	return o != nullptr
		? Lookup(*o, key)
		: nullptr;
}

[[gnu::pure]]
static inline const auto *
Lookup(const boost::json::array &a, std::size_t i) noexcept
{
	return a.if_contains(i);
}

[[gnu::pure]]
static inline const auto *
Lookup(const boost::json::value &v, std::size_t i) noexcept
{
	const auto *a = v.if_array();
	return a != nullptr
		? Lookup(*a, i)
		: nullptr;
}

template<typename J, typename K, typename... Args>
[[gnu::pure]]
static inline const auto *
Lookup(const J &j, K &&key, Args&&... args)
{
	const auto *l = Lookup(j, std::forward<K>(key));
	return l != nullptr
		? Lookup(*l, std::forward<Args>(args)...)
		: nullptr;
}

template<typename... Args>
[[gnu::pure]]
static inline const auto *
LookupObject(Args&&... args)
{
	const auto *o = Lookup(std::forward<Args>(args)...);
	return o != nullptr
		? o->if_object()
		: nullptr;
}

} // namespace Json
