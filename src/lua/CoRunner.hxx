// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Thread.hxx"

namespace Lua {

class ResumeListener;

/**
 * A class which helps with running code in a Lua thread (coroutine).
 */
class CoRunner {
	/**
	 * The Lua thread where the function runs.
	 */
	Thread thread;

public:
	explicit CoRunner(lua_State *L) noexcept
		:thread(L) {}

	lua_State *CreateThread(ResumeListener &listener);

	/**
	 * Push the thread object to the given #lua_State stack.
	 */
	void Push(lua_State *L) {
		thread.Push(L);
	}

	void Cancel();

	lua_State *GetMainState() const noexcept {
		return thread.GetMainState();
	}
};

} // namespace Lua
