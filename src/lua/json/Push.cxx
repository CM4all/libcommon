// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Push.hxx"
#include "lua/Util.hxx"

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
