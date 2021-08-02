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
	virtual void OnLuaFinished() noexcept = 0;
	virtual void OnLuaError(std::exception_ptr e) noexcept = 0;
};

/**
 * Installs a #ResumeListener in the given Lua thread.
 */
void
SetResumeListener(lua_State *L, ResumeListener &listener) noexcept;

/**
 * Call lua_resume() and invoke the #ResumeListener on completion.
 *
 * If lua_resume()==LUA_YIELD, do nothing - the entity which called
 * lua_yield() is responsible for calling Resume() again eventually.
 */
void
Resume(lua_State *L, int narg) noexcept;

} // namespace Lua
