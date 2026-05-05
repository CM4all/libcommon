// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Parse.hxx"
#include "ToJson.hxx"
#include "Push.hxx"
#include "lua/CheckArg.hxx"
#include "lua/Error.hxx"
#include "lua/Util.hxx"

#include <nlohmann/json.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Lua {

int
ParseJson(lua_State *L)
try {
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Not enough parameters");

	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto s = CheckStringView(L, 1);
	const auto j = nlohmann::json::parse(s);

	Push(L, j);
	return 1;
} catch (...) {
	if (auto e = std::current_exception()) {
		// return [nil, error_message] for assert()
		Push(L, nullptr);
		Push(L, std::current_exception());
		return 2;
	} else
		throw;
}

} // namespace Lua
