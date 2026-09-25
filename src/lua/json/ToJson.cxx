// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ToJson.hxx"
#include "lua/AbsoluteStackIndex.hxx"
#include "lua/ForEach.hxx"
#include "lua/StringView.hxx"
#include "util/ScopeExit.hxx"

#include <nlohmann/json.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <stdexcept>

#include <stdio.h>

namespace Lua {

/**
 * The maximum table nesting depth; deeper structures are rejected to
 * avoid stack overflow.
 */
static constexpr unsigned MAX_JSON_DEPTH = 64;

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
ToJson(lua_State *L, int idx, unsigned depth);

static nlohmann::json
TableToJson(lua_State *L, const int _idx, const unsigned depth)
{
	if (depth >= MAX_JSON_DEPTH)
		throw std::runtime_error{"JSON nesting too deep"};

	/* if the caller passes a negative number, convert it to an
	   absolute stack index because ForEach() requires that */
	const auto idx = ToAbsoluteStackIndex(L, _idx);

	// TODO array?

        auto o = nlohmann::json::object();

	ForEach(L, idx, [L, depth, &o](auto key_idx, auto value_idx){
		auto value = ToJson(L, GetStackIndex(value_idx), depth + 1);

		lua_pushvalue(L, GetStackIndex(key_idx));
		AtScopeExit(L) { lua_pop(L, 1); };

		const auto key = ToStringView(L, -1);
		o.emplace(key, std::move(value));
	});

	return o;
}

static nlohmann::json
ToJson(lua_State *L, const int idx, const unsigned depth)
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
		return TableToJson(L, idx, depth);

	case LUA_TFUNCTION:
		return FunctionToJson(L, idx);

	case LUA_TTHREAD:
		return ThreadToJson(L, idx);
	}

	// TODO what now?
	return {};
}

nlohmann::json
ToJson(lua_State *L, int idx)
{
	return ToJson(L, idx, 0);
}

} // namespace Lua
