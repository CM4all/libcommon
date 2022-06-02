/*
 * Copyright 2021-2022 CM4all GmbH
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

#include "CoRunner.hxx"
#include "Resume.hxx"

namespace Lua {

lua_State *
CoRunner::CreateThread(ResumeListener &listener)
{
	const auto main_L = GetMainState();
	const ScopeCheckStack check_main_stack(main_L);

	/* create a new thread for the coroutine */
	const auto L = lua_newthread(main_L);
	thread.Set(main_L, RelativeStackIndex{-1});
	/* pop the new thread from the main stack */
	lua_pop(main_L, 1);

	SetResumeListener(L, listener);
	return L;
}

void
CoRunner::Cancel()
{
	const auto main_L = GetMainState();
	const ScopeCheckStack check_main_stack(main_L);

	thread.Push(main_L);
	thread.Set(main_L, nullptr);

	bool need_gc = false;
	if (const auto L = lua_tothread(main_L, -1); L != nullptr) {
		const ScopeCheckStack check_thread_stack(L);
		need_gc = UnsetResumeListener(L) != nullptr;
	}

	lua_pop(main_L, 1);

	if (need_gc)
		/* force a full GC so all pending operations are
		   cancelled */
		// TODO: is there a more elegant way without forcing a full GC?
		lua_gc(main_L, LUA_GCCOLLECT, 0);
}

} // namespace Lua
