// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>

struct lua_State;

namespace Lua {

/**
 * An interface which receives a callback when the Lua coroutine
 * completes (lua_resume() returns something other than LUA_YIELD).
 */
class ResumeListener {
public:
	virtual void OnLuaFinished(lua_State *L) noexcept = 0;
	virtual void OnLuaError(lua_State *L,
				std::exception_ptr e) noexcept = 0;
};

/**
 * Installs a #ResumeListener in the given Lua thread.
 */
void
SetResumeListener(lua_State *L, ResumeListener &listener) noexcept;

/**
 * Uninstalls the #ResumeListener (if one exists).  Returns a pointer
 * or nullptr if there was no #ResumeListener.
 */
ResumeListener *
UnsetResumeListener(lua_State *L) noexcept;

/**
 * Call lua_resume() and invoke the #ResumeListener on completion.
 *
 * If lua_resume()==LUA_YIELD, do nothing - the entity which called
 * lua_yield() is responsible for calling Resume() again eventually.
 */
void
Resume(lua_State *L, int narg) noexcept;

} // namespace Lua
