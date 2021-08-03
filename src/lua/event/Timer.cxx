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

#include "Init.hxx"
#include "Timer.hxx"
#include "lua/Class.hxx"
#include "lua/PushCClosure.hxx"
#include "lua/Resume.hxx"
#include "lua/Util.hxx"
#include "event/FineTimerEvent.hxx"

extern "C" {
#include <lauxlib.h>
}

namespace Lua {

class Timer final {
	lua_State *const L;

	FineTimerEvent timer_event;

public:
	Timer(lua_State *_L, EventLoop &event_loop,
	      Event::Duration duration) noexcept
		:L(_L),
		 timer_event(event_loop, BIND_THIS_METHOD(OnTimer))
	{
		timer_event.Schedule(duration);
	}

private:
	void OnTimer() noexcept {
		Resume(L, 0);
	}
};

static constexpr char lua_timer_class[] = "Timer";
using TimerClass = Lua::Class<Timer, lua_timer_class>;

static int
Sleep(lua_State *L)
{
	auto &event_loop = *(EventLoop *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto seconds = luaL_checknumber(L, 1);
	if (seconds < 0)
		luaL_argerror(L, 1, "Positive number expected");

	const std::chrono::duration<lua_Number> duration{seconds};

	TimerClass::New(L, L, event_loop,
			std::chrono::duration_cast<Event::Duration>(duration));
	return lua_yield(L, 1);
}

void
InitTimer(lua_State *L, EventLoop &event_loop) noexcept
{
	const ScopeCheckStack check_stack{L};

	TimerClass::Register(L);
	lua_pop(L, 1);

	SetGlobal(L, "sleep",
		  Lua::MakeCClosure(Sleep, Lua::LightUserData(&event_loop)));
}

} // namespace Lua
