// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Array.hxx"
#include "lua/ForEach.hxx"
#include "lua/StringView.hxx"
#include "lua/Util.hxx"
#include "pg/Array.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <stdexcept>
#include <vector>

namespace Lua {

int
EncodeArray(lua_State *L)
{
	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	luaL_argcheck(L, lua_istable(L, 2), 2, "Table expected");

	const std::size_t n = lua_objlen(L, 2);

	std::vector<std::string_view> list{n};

	ForEach(L, 2, [L, &list, n](auto key_idx, auto value_idx){
		const ScopeCheckStack check_stack{L};

		if (!lua_isnumber(L, GetStackIndex(key_idx)))
			throw std::invalid_argument{"Key is not a number"};

		const int idx = lua_tointeger(L, GetStackIndex(key_idx));
		if (idx < 1 || (std::size_t)idx > n)
			throw std::invalid_argument{"Key out of range"};

		const auto value = ToStringView(L, GetStackIndex(value_idx));
		if (value.data() == nullptr)
			throw std::invalid_argument{"Bad value"};

		list[idx - 1] = value;
	});

	Push(L, Pg::EncodeArray(list));
	return 1;
}

int
DecodeArray(lua_State *L)
{
	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	const auto list = Pg::DecodeArray(luaL_checkstring(L, 2));

	lua_newtable(L);

	int i = 1;
	for (const auto &value : list)
		RawSet(L, RelativeStackIndex{-1}, i++, value);

	return 1;
}

} // namespace Lua
