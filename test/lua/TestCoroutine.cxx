// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lua/Resume.hxx"
#include "lua/Error.hxx"
#include "lua/State.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

namespace {

struct Instance;

struct MyThread final : Lua::ResumeListener {
	lua_State *const L;

	std::exception_ptr error;

	bool finished = false;

	MyThread(lua_State *_L) noexcept
		:L(lua_newthread(_L))
	{
		Lua::SetResumeListener(L, *this);

		/* pop the lua_newthread() (a reference to it is held
		   by SetResumeListener()) */
		lua_pop(_L, 1);
	}

	~MyThread() noexcept {
		Lua::UnsetResumeListener(L);
	}

	void Do(const char *code) const {
		if (luaL_loadstring(L, code))
			throw Lua::PopError(L);

		Lua::Resume(L, 0);
	}

	void OnLuaFinished(lua_State *) noexcept override {
		finished = true;
	}

	void OnLuaError(lua_State *, std::exception_ptr e) noexcept override {
		finished = true;
		error = std::move(e);
	}
};

static bool
GetBool(lua_State *L, const char *name)
{
	lua_getglobal(L, name);
	const bool value = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return value;

}

} // anonymous namespace

TEST(LuaCoroutine, Resume)
{
	const Lua::State main{luaL_newstate()};
	lua_State *const L = main.get();
	luaL_openlibs(L);
	Lua::InitResume(L);

	/* the first coroutine yields twice, i.e. must be resumed
	   twice */
	MyThread t1{L};
	t1.Do(R"(
waiting = coroutine.running()
if coroutine.status(waiting) ~= 'running' then error(coroutine.status(waiting)) end
coroutine.yield()
finished1a = true
if coroutine.status(waiting) ~= 'running' then error(coroutine.status(waiting)) end
coroutine.yield()
if coroutine.status(waiting) ~= 'running' then error(coroutine.status(waiting)) end
finished1b = true
)");

	EXPECT_FALSE(t1.finished);
	EXPECT_FALSE(t1.error);
	EXPECT_FALSE(GetBool(L, "finished1a"));
	EXPECT_FALSE(GetBool(L, "finished1b"));
	EXPECT_FALSE(GetBool(L, "finished2"));

	/* wake up the coroutine once */
	if (luaL_dostring(L, "coroutine.resume(waiting)"))
		throw Lua::PopError(L);

	EXPECT_FALSE(t1.finished);
	EXPECT_FALSE(t1.error);
	EXPECT_TRUE(GetBool(L, "finished1a"));
	EXPECT_FALSE(GetBool(L, "finished1b"));
	EXPECT_FALSE(GetBool(L, "finished2"));

	/* wake up the coroutine again from a new coroutine */
	MyThread t2{L};
	t2.Do(R"(
if coroutine.status(waiting) ~= 'suspended' then error(coroutine.status(waiting)) end
coroutine.resume(waiting)
if coroutine.status(waiting) ~= 'dead' then error(coroutine.status(waiting)) end
finished2 = true
)");

	EXPECT_TRUE(t1.finished);
	EXPECT_FALSE(t1.error);
	EXPECT_TRUE(t2.finished);
	EXPECT_FALSE(t2.error);
	EXPECT_TRUE(GetBool(L, "finished1a"));
	EXPECT_TRUE(GetBool(L, "finished1b"));
	EXPECT_TRUE(GetBool(L, "finished2"));
}

TEST(LuaCoroutine, ResumeError)
{
	const Lua::State main{luaL_newstate()};
	lua_State *const L = main.get();
	luaL_openlibs(L);
	Lua::InitResume(L);

	/* the coroutine yields and then throws a Lua error */
	MyThread t1{L};
	t1.Do(R"(
waiting = coroutine.running()
coroutine.yield()
finished1 = true
error("foo")
)");

	EXPECT_FALSE(t1.finished);
	EXPECT_FALSE(t1.error);
	EXPECT_FALSE(GetBool(L, "finished1"));
	EXPECT_FALSE(GetBool(L, "finished2"));

	MyThread t2{L};
	t2.Do(R"(
coroutine.resume(waiting)
finished2 = true
)");

	EXPECT_TRUE(t1.finished);
	EXPECT_TRUE(t1.error);
	EXPECT_TRUE(t2.finished);
	EXPECT_FALSE(t2.error);
	EXPECT_TRUE(GetBool(L, "finished1"));
	EXPECT_TRUE(GetBool(L, "finished2"));
}
