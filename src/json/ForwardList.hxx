// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json.hpp>

#include <forward_list>

namespace std {

/**
 * Convert a JSON array to a std::forward_list<T>.
 */
template<typename T>
auto
tag_invoke(boost::json::value_to_tag<forward_list<T>>,
	   const boost::json::value &jv)
{
	const auto &array = jv.as_array();
	forward_list<T> result;
	auto j = result.before_begin();
	for (const auto &i : array)
		j = result.emplace_after(j, boost::json::value_to<T>(i));

	return result;
}

} // namespace std
