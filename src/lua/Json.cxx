/*
 * Copyright 2022 CM4all GmbH
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

#include "Json.hxx"
#include "Util.hxx"

#include <boost/json/value.hpp>

namespace Lua {

void
Push(lua_State *L, const boost::json::string &s)
{
	Push(L, (std::string_view)s);
}

void
Push(lua_State *L, const boost::json::array &a)
{
	lua_newtable(L);

	for (std::size_t i = 0, n = a.size(); i != n; ++i) {
		Push(L, a[i]);
		lua_rawseti(L, -2, i + 1);
	}
}

void
Push(lua_State *L, const boost::json::object &o)
{
	lua_newtable(L);

	for (const auto &i : o)
		SetTable(L, RelativeStackIndex{-1},
			 i.key(), i.value());
}

void
Push(lua_State *L, const boost::json::value &value)
{
	switch (value.kind()) {
	case boost::json::kind::null:
		break;

	case boost::json::kind::bool_:
		Push(L, value.get_bool());
		return;

	case boost::json::kind::double_:
		Push(L, value.get_double());
		return;

	case boost::json::kind::array:
		Push(L, value.get_array());
		return;

	case boost::json::kind::object:
		Push(L, value.get_object());
		return;

	case boost::json::kind::int64:
		Push(L, (lua_Integer)value.get_int64());
		return;

	case boost::json::kind::uint64:
		Push(L, (lua_Integer)value.get_uint64());
		return;

	case boost::json::kind::string:
		Push(L, value.get_string());
		return;
	}

	lua_pushnil(L);
}

} // namespace Lua
