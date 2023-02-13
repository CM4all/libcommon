// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Value.hxx"

struct lua_State;

namespace Lua {

class ResumeListener;

/**
 * A class which helps with running code in a Lua thread (coroutine).
 */
class CoRunner {
	/**
	 * The Lua thread where the function runs.
	 */
	Lua::Value thread;

public:
	explicit CoRunner(lua_State *L) noexcept
		:thread(L) {}

	lua_State *CreateThread(ResumeListener &listener);

	void Cancel();

	lua_State *GetMainState() const noexcept {
		return thread.GetState();
	}
};

} // namespace Lua
