// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lua/CoRunner.hxx"
#include "lua/Resume.hxx"
#include "lua/Error.hxx"
#include "lua/State.hxx"
#include "lua/event/Timer.hxx"
#include "event/Loop.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

struct MyResumeListener final : Lua::ResumeListener {
	std::exception_ptr error;
	bool done = false;

	/* virtual methods from class Lua::ResumeListener */
	void OnLuaFinished(lua_State *) noexcept override {
		assert(!done);
		assert(!error);

		done = true;
	}

	void OnLuaError(lua_State *, std::exception_ptr e) noexcept override {
		assert(!done);
		assert(!error);

		done = true;
		error = std::move(e);
	}
};

TEST(LuaCoRunner, Basic)
{
	const Lua::State main{luaL_newstate()};

	EventLoop event_loop;
	Lua::InitTimer(main.get(), event_loop);

	if (luaL_dostring(main.get(), "function foo() sleep(0) end"))
		throw Lua::PopError(main.get());

	MyResumeListener l;

	Lua::CoRunner thread{main.get()};
	const auto thread_L = thread.CreateThread(l);
	lua_pop(main.get(), 1);

	ASSERT_FALSE(l.done);

	lua_getglobal(thread_L, "foo");
	ASSERT_TRUE(lua_isfunction(thread_L, -1));

	Lua::Resume(thread_L, 0);
	ASSERT_FALSE(l.done);

	event_loop.Run();
	ASSERT_TRUE(l.done);

	/* nothing to cancel, but let's try this code path anyway */
	thread.Cancel();
}

TEST(LuaCoRunner, Cancel)
{
	const Lua::State main{luaL_newstate()};

	EventLoop event_loop;
	Lua::InitTimer(main.get(), event_loop);

	if (luaL_dostring(main.get(), "function foo() sleep(1) end"))
		throw Lua::PopError(main.get());

	MyResumeListener l;

	Lua::CoRunner thread{main.get()};
	const auto thread_L = thread.CreateThread(l);
	lua_pop(main.get(), 1);

	lua_getglobal(thread_L, "foo");
	ASSERT_TRUE(lua_isfunction(thread_L, -1));

	Lua::Resume(thread_L, 0);
	ASSERT_FALSE(l.done);

	thread.Cancel();

	/* this must not block because the timer must be canceled */
	event_loop.Run();
}
