// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lua/Chrono.hxx"
#include "lua/Assert.hxx"
#include "lua/State.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
}

using namespace Lua;

TEST(Chrono, Duration)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	Push(L, std::chrono::microseconds{5});
	Push(L, std::chrono::milliseconds{4});
	Push(L, std::chrono::hours{3});
	Push(L, std::chrono::minutes{2});
	Push(L, std::chrono::seconds{1});

	EXPECT_EQ(ToDuration(L, -1), std::chrono::seconds{1});
	EXPECT_EQ(ToDuration(L, -2), std::chrono::minutes{2});
	EXPECT_EQ(ToDuration(L, -3), std::chrono::hours{3});
	EXPECT_EQ(ToDuration(L, -4), std::chrono::milliseconds{4});
	EXPECT_EQ(ToDuration(L, -5), std::chrono::microseconds{5});

	lua_pop(L, 5);
}

TEST(Chrono, SystemClock)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	const auto one = std::chrono::system_clock::from_time_t(1234567890);
	const auto now = std::chrono::system_clock::now();

	Push(L, one);
	Push(L, now);

	EXPECT_EQ(ToSystemTimePoint(L, -2), one);

	/* comparing through time_t because our Push() implementation
	   doesn't support sub-second precision */
	EXPECT_EQ(std::chrono::system_clock::to_time_t(ToSystemTimePoint(L, -1)),
		  std::chrono::system_clock::to_time_t(now));

	lua_pop(L, 2);
}
