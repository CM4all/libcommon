// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lua/Class.hxx"
#include "lua/Assert.hxx"
#include "lua/State.hxx"
#include "lua/Util.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <exception> // for std::terminate()

using namespace Lua;

TEST(Class, Empty)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	struct T {};
	static constexpr char name[] = "T";
	using C = Class<T, name>;

	C::Register(L);
	lua_pop(L, 1);

	C::New(L);
	lua_pop(L, 1);

	lua_gc(L, LUA_GCCOLLECT, 0);
}

TEST(Class, Int)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	static constexpr char name[] = "int";
	using C = Class<int, name>;

	C::Register(L);
	lua_pop(L, 1);

	auto *p = C::New(L, 42);
	EXPECT_EQ(*p, 42);
	lua_pop(L, 1);

	lua_gc(L, LUA_GCCOLLECT, 0);
}

// see if the destructor gets called
TEST(Class, Dtor)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	unsigned n = 0;

	struct T {
		unsigned &n_;

		explicit constexpr T(unsigned &_n) noexcept:n_(_n) {}

		~T() noexcept {
			++n_;
		}

		T(const T &) = delete;
		T &operator=(const T &) = delete;
	};

	static constexpr char name[] = "T";
	using C = Class<T, name>;

	C::Register(L);
	lua_pop(L, 1);

	C::New(L, n);
	EXPECT_EQ(n, 0);
	lua_pop(L, 1);

	EXPECT_EQ(n, 0);

	/* the GC calls the destructor */
	lua_gc(L, LUA_GCCOLLECT, 0);
	EXPECT_EQ(n, 1);
}

// constructor throws C++ exception
TEST(Class, Throw)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	struct T {
		[[noreturn]]
		T() {
			throw 42;
		}

		[[noreturn]]
		~T() noexcept {
			// unreachable because the constructor throws
			std::terminate();
		}
	};

	static constexpr char name[] = "T";
	using C = Class<T, name>;

	C::Register(L);
	lua_pop(L, 1);

	try {
		C::New(L);
	} catch (int e) {
		EXPECT_EQ(e, 42);
	}

	lua_gc(L, LUA_GCCOLLECT, 0);
}

// constructor throws Lua error via lua_error()
TEST(Class, Error)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	struct T {
		T(lua_State *L) {
			lua_pushstring(L, "foo");
			lua_error(L);
		}

		[[noreturn]]
		~T() noexcept {
			// unreachable because the constructor errors
			std::terminate();
		}
	};

	static constexpr char name[] = "T";
	using C = Class<T, name>;

	C::Register(L);
	lua_pop(L, 1);

	try {
		C::New(L, L);
	} catch (...) {
		EXPECT_FALSE(std::exception_ptr());
		EXPECT_TRUE(lua_isstring(L, -1));
		EXPECT_STREQ(lua_tostring(L, -1), "foo");
		lua_pop(L, 1);
	}

	lua_gc(L, LUA_GCCOLLECT, 0);
}
