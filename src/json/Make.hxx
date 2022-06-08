/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
