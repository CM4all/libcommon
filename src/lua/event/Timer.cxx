// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

	void Cancel() noexcept {
		timer_event.Cancel();
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

	SetField(L, RelativeStackIndex{-1}, "__close", [](auto _L){
		auto &timer = TimerClass::Cast(_L, 1);
		timer.Cancel();
		return 0;
	});

	lua_pop(L, 1);

	SetGlobal(L, "sleep",
		  Lua::MakeCClosure(Sleep, Lua::LightUserData(&event_loop)));
}

} // namespace Lua
