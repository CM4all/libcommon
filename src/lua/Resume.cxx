// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Resume.hxx"
#include "Error.hxx"
#include "Util.hxx"

#include <cassert>

namespace Lua {

static constexpr auto resume_listener_key = "ResumeListener";

void
SetResumeListener(lua_State *L, ResumeListener &listener) noexcept
{
	const ScopeCheckStack check_stack(L);

	/* look up registry[key] */
	lua_getfield(L, LUA_REGISTRYINDEX, resume_listener_key);
	if (lua_isnil(L, -1)) {
		/* pop nil */
		lua_pop(L, 1);

		/* create a new table */
		lua_newtable(L);
		/* leave a copy on the stack (because lua_setfield()
		   pops it) */
		Push(L, RelativeStackIndex{-1});
		/* registry[key] = newtable */
		lua_setfield(L, LUA_REGISTRYINDEX, resume_listener_key);
	}

	/* registry[key][L] = &listener */
	SetTable(L, RelativeStackIndex{-1},
		 CurrentThread{}, LightUserData{&listener});

	/* pop table */
	lua_pop(L, 1);
}

ResumeListener *
UnsetResumeListener(lua_State *L) noexcept
{
	const ScopeCheckStack check_stack(L);

	/* look up registry[key] */
	lua_getfield(L, LUA_REGISTRYINDEX, resume_listener_key);
	if (lua_isnil(L, -1)) {
		/* pop nil */
		lua_pop(L, 1);
		return nullptr;
	}

	assert(lua_istable(L, -1));

	/* look up registry[key][L] */
	GetTable(L, RelativeStackIndex{-1}, CurrentThread{});
	if (lua_isnil(L, -1)) {
		/* pop nil and registry[key] */
		lua_pop(L, 2);
		return nullptr;
	}

	assert(lua_islightuserdata(L, -1));

	auto *listener = (ResumeListener *) lua_touserdata(L, -1);
	assert(listener != nullptr);

	/* registry[key][L] = nil */
	SetTable(L, RelativeStackIndex{-2}, CurrentThread{}, nullptr);

	/* pop listener and registry[key] */
	lua_pop(L, 2);

	return listener;
}

void
Resume(lua_State *L, int narg) noexcept
{
	try {
		int result = lua_resume(L, narg);
		if (result == LUA_OK) {
			auto *listener = UnsetResumeListener(L);
			assert(listener != nullptr);
			listener->OnLuaFinished(L);
		} else if (result != LUA_YIELD)
			throw PopError(L);
	} catch (...) {
		auto *listener = UnsetResumeListener(L);
		assert(listener != nullptr);
		listener->OnLuaError(L, std::current_exception());
	}
}

} // namespace Lua
