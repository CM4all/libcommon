/*
 * Copyright 2021 CM4all GmbH
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
	if (lua_isnil(L, 1)) {
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

static ResumeListener &
UnsetResumeListener(lua_State *L) noexcept
{
	const ScopeCheckStack check_stack(L);

	/* look up registry[key] */
	lua_getfield(L, LUA_REGISTRYINDEX, resume_listener_key);
	assert(lua_istable(L, -1));

	/* look up registry[key][L] */
	GetTable(L, RelativeStackIndex{-1}, CurrentThread{});
	assert(lua_islightuserdata(L, -1));

	auto *listener = (ResumeListener *) lua_touserdata(L, -1);
	assert(listener != nullptr);

	/* registry[key][L] = nil */
	SetTable(L, RelativeStackIndex{-2}, CurrentThread{}, nullptr);

	/* pop listener and registry[key] */
	lua_pop(L, 2);

	return *listener;
}

void
Resume(lua_State *L, int narg) noexcept
{
	try {
		int result = lua_resume(L, narg);
		if (result == LUA_OK)
			UnsetResumeListener(L).OnLuaFinished();
		else if (result != LUA_YIELD)
			throw PopError(L);
	} catch (...) {
		UnsetResumeListener(L).OnLuaError(std::current_exception());
	}
}

} // namespace Lua
