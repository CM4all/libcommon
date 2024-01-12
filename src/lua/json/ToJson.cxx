// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ToJson.hxx"
#include "lua/ForEach.hxx"
#include "lua/StringView.hxx"
#include "util/ScopeExit.hxx"

#include <nlohmann/json.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <stdio.h>

namespace Lua {

static nlohmann::json
PointerToJson(const char *prefix, const void *ptr) noexcept
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%s:%p", prefix, ptr);
	return buffer;
}

static nlohmann::json
UserDataToJson(lua_State *L, int idx) noexcept
{
	return PointerToJson("userdata", lua_touserdata(L, idx));
}

static nlohmann::json
FunctionToJson(lua_State *L, int idx) noexcept
{
	return PointerToJson("cfunction",
			     (const void *)lua_tocfunction(L, idx));
}

static nlohmann::json
ThreadToJson(lua_State *L, int idx) noexcept
{
	return PointerToJson("thread", lua_tothread(L, idx));
}

static nlohmann::json
TableToJson(lua_State *L, int idx) noexcept
{
	// TODO array?

        auto o = nlohmann::json::object();

	ForEach(L, idx, [L, &o](auto key_idx, auto value_idx){
		auto value = ToJson(L, GetStackIndex(value_idx));

		lua_pushvalue(L, GetStackIndex(key_idx));
		AtScopeExit(L) { lua_pop(L, 1); };

		const auto key = ToStringView(L, -1);
		o.emplace(key, std::move(value));
	});

	return o;
}

nlohmann::json
ToJson(lua_State *L, int idx) noexcept
{
	switch (lua_type(L, idx)) {
	case LUA_TNIL:
		return nullptr;

	case LUA_TBOOLEAN:
		return lua_toboolean(L, idx);

	case LUA_TLIGHTUSERDATA:
	case LUA_TUSERDATA:
		return UserDataToJson(L, idx);

	case LUA_TNUMBER:
		// TODO what about floating point?
		return lua_tointeger(L, idx);

	case LUA_TSTRING:
		return ToStringView(L, idx);

	case LUA_TTABLE:
		return TableToJson(L, idx);

	case LUA_TFUNCTION:
		return FunctionToJson(L, idx);

	case LUA_TTHREAD:
		return ThreadToJson(L, idx);
	}

	// TODO what now?
	return {};
}

static int
ToJson(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Not enough parameters");

	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto json = ToJson(L, 1).dump();
	lua_pushlstring(L, json.data(), json.size());
	return 1;
}

void
InitToJson(lua_State *L) noexcept
{
	lua_pushcfunction(L, ToJson);
	lua_setglobal(L, "to_json");
}

} // namespace Lua
