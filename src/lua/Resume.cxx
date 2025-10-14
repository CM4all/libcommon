// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Resume.hxx"
#include "Error.hxx"
#include "LightUserData.hxx"
#include "PushCClosure.hxx"
#include "RegistryTable.hxx"
#include "Util.hxx"

#include <cassert>

namespace Lua {

/**
 * A global variable used to build a unique LightUserData for storing
 * the #ResumeListener table.
 */
static int resume_listener_id;

[[gnu::pure]]
static bool
IsCoroutineFinished(lua_State *L) noexcept
{
	/* the coroutine is finished if there is nothing at the
	   top-most call stack level */
	lua_Debug d;
	return lua_getstack(L, 0, &d) == 0;
}

/**
 * This functions is a wrapper for coroutine.resume() which invokes
 * the #ResumeListener if the specified coroutine finishes.
 *
 * Usually, the #ResumeListener gets invoked by this library's C++
 * Resume() function, but if Lua code yields and resumes manually,
 * nobody invokes the C++ function.  To be able to observe Lua
 * coroutines resumed by coroutine.resume(), we wrap this function.
 */
static int
ResumeWrapper(lua_State *L)
{
	int top = lua_gettop(L);
	lua_State *co = lua_tothread(L, 1);

	// get original resume function from upvalue
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);

	// call original resume
	if (lua_pcall(L, top, LUA_MULTRET, 0) != LUA_OK) {
		// Error in coroutine.resume itself (shouldn't normally happen)
		if (co != nullptr)
			if (auto *listener = UnsetResumeListener(co))
				listener->OnLuaError(co, std::make_exception_ptr(Error{co}));

		// re-throw to the coroutine.resume() caller
		return lua_error(L);
	}

	// Check the first return value (success flag from coroutine.resume)
	const int nresults = lua_gettop(L);
	if (co != nullptr && nresults > 0 && lua_isboolean(L, 1)) {
		const bool success = lua_toboolean(L, 1);
		if (!success) {
			// Coroutine had an error
			if (auto *listener = UnsetResumeListener(co))
				listener->OnLuaError(co, std::make_exception_ptr(Error{L, 2}));
		} else if (lua_status(co) == LUA_OK) {
			// Coroutine completed successfully
			if (IsCoroutineFinished(co))
				if (auto *listener = UnsetResumeListener(co))
					listener->OnLuaFinished(co);
		}
	}

	return nresults;
}

void
InitResume(lua_State *L) noexcept
{
	const ScopeCheckStack check_stack{L};

	lua_getglobal(L, "coroutine");
	if (!lua_istable(L, -1)) {
		// coroutine library not loaded
		lua_pop(L, 1);
		return;
	}

	// save original resume
	lua_getfield(L, -1, "resume");
	if (!lua_isfunction(L, -1)) {
		// coroutine.resume() does not exist
		lua_pop(L, 2);
		return;
	}

	SetField(L, RelativeStackIndex{-2}, "resume",
		 MakeCClosure(ResumeWrapper, RelativeStackIndex{-1}));

	// pop "resume" and "coroutine"
	lua_pop(L, 2);
}

void
SetResumeListener(lua_State *L, ResumeListener &listener) noexcept
{
	const ScopeCheckStack check_stack(L);

	/* look up registry[key] */
	MakeRegistryTable(L, LightUserData{&resume_listener_id});

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
	GetTable(L, LUA_REGISTRYINDEX, LightUserData{&resume_listener_id});
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
