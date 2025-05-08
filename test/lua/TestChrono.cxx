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
