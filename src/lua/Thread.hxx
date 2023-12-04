// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Value.hxx"
#include "util/Concepts.hxx"

namespace Lua {

/**
 * A wrapper for a Lua thread (= coroutine).  Call Create() to
 * actually create the thread.
 */
class Thread {
	Lua::Value thread;

public:
	explicit Thread(lua_State *L) noexcept
		:thread(L) {}

	lua_State *GetMainState() const noexcept {
		return thread.GetState();
	}

	/**
	 * Wrapper for lua_newthread().  Returns the new thread state.
	 */
	lua_State *Create(lua_State *main_L) noexcept {
		const ScopeCheckStack check_main_stack{main_L, 1};

		auto *thread_L = lua_newthread(main_L);
		thread.Set(main_L, RelativeStackIndex{-1});
		return thread_L;
	}

	lua_State *Create() noexcept {
		return Create(GetMainState());
	}

	/**
	 * Push the thread object to the given #lua_State stack.
	 */
	void Push(lua_State *L) {
		thread.Push(L);
	}

	/**
	 * Drop the thread reference and invoke the given disposer
	 * function.
	 */
	void Dispose(lua_State *main_L,
		     Disposer<lua_State> auto disposer) noexcept {
		const ScopeCheckStack check_main_stack{main_L};

		thread.Push(main_L);
		thread.Set(main_L, nullptr);

		if (auto *thread_L = lua_tothread(main_L, -1);
		    thread_L != nullptr) {
			const ScopeCheckStack check_thread_stack{thread_L};
			disposer(thread_L);
		}

		lua_pop(main_L, 1);
	}

	void Dispose(Disposer<lua_State> auto disposer) noexcept {
		Dispose(GetMainState(), disposer);
	}
};

} // namespace Lua
