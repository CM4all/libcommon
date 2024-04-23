// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lua/CoAwaitable.hxx"
#include "lua/Error.hxx"
#include "lua/State.hxx"
#include "lua/Thread.hxx"
#include "lua/event/Timer.hxx"
#include "event/Loop.hxx"
#include "co/SimpleTask.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

static Co::SimpleTask
CompletionCallback(bool &flag) noexcept
{
	flag = true;
	co_return;
}

TEST(LuaCoAwaitable, Basic)
{
	const Lua::State main{luaL_newstate()};

	EventLoop event_loop;
	Lua::InitTimer(main.get(), event_loop);

	if (luaL_dostring(main.get(), "function foo() sleep(0) end"))
		throw Lua::PopError(main.get());

	Lua::Thread thread{main.get()};
	const auto thread_L = thread.Create();
	lua_pop(main.get(), 1);

	lua_getglobal(thread_L, "foo");
	ASSERT_TRUE(lua_isfunction(thread_L, -1));

	Lua::CoAwaitable awaitable{thread, thread_L, 0};

	ASSERT_FALSE(awaitable.await_ready());

	bool complete = false;
	Co::UniqueHandle<> callback = CompletionCallback(complete);

	ASSERT_FALSE(complete);

	awaitable.await_suspend(callback.get());

	ASSERT_FALSE(complete);

	event_loop.Run();

	ASSERT_TRUE(complete);

	awaitable.await_resume();
}

TEST(LuaCoAwaitable, Cancel)
{
	const Lua::State main{luaL_newstate()};

	EventLoop event_loop;
	Lua::InitTimer(main.get(), event_loop);

	if (luaL_dostring(main.get(), "function foo() sleep(1) end"))
		throw Lua::PopError(main.get());

	{
		Lua::Thread thread{main.get()};
		const auto thread_L = thread.Create();
		lua_pop(main.get(), 1);

		lua_getglobal(thread_L, "foo");
		ASSERT_TRUE(lua_isfunction(thread_L, -1));

		Lua::CoAwaitable awaitable{thread, thread_L, 0};

		ASSERT_FALSE(awaitable.await_ready());

		bool complete = false;
		Co::UniqueHandle<> callback = CompletionCallback(complete);

		ASSERT_FALSE(complete);

		awaitable.await_suspend(callback.get());

		ASSERT_FALSE(complete);

		// leaving this scope cancels the operation
	}

	/* this must not block because the timer must be canceled */
	event_loop.Run();
}
