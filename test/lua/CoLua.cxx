/*
 * Copyright 2021-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "lua/RunFile.hxx"
#include "lua/Util.hxx"
#include "lua/Error.hxx"
#include "lua/Resume.hxx"
#include "lua/event/Init.hxx"
#include "lua/pg/Init.hxx"
#include "event/Loop.hxx"
#include "event/DeferEvent.hxx"
#include "event/ShutdownListener.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <stdio.h>
#include <stdlib.h>

using namespace Lua;

struct Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	Instance() noexcept
	{
		shutdown_listener.Enable();
	}

	void OnShutdown() noexcept {
		event_loop.Break();
	}
};

class Thread final : ResumeListener {
	lua_State *const L;

	DeferEvent start_event;

	const char *const path;

	std::exception_ptr error;

public:
	Thread(lua_State *_L, EventLoop &event_loop,
	       const char *_path) noexcept
		:L(lua_newthread(_L)),
		 start_event(event_loop, BIND_THIS_METHOD(Start)),
		 path(_path)
	{
		SetResumeListener(L, *this);

		/* pop the lua_newthread() (a reference to it is held
		   by SetResumeListener()) */
		lua_pop(L, 1);

		start_event.Schedule();
	}

	~Thread() noexcept {
		UnsetResumeListener(L);
	}

	auto &GetEventLoop() const noexcept {
		return start_event.GetEventLoop();
	}

	void CheckRethrow() const {
		if (error)
			std::rethrow_exception(error);
	}

private:
	void Start() noexcept {
		try {
			if (luaL_loadfile(L, path))
				throw PopError(L);

			Resume(L, 0);
		} catch (...) {
			OnLuaError(L, std::current_exception());
		}
	}

	void OnLuaFinished(lua_State *) noexcept override {
		GetEventLoop().Break();
	}

	void OnLuaError(lua_State *, std::exception_ptr e) noexcept override {
		error = std::move(e);
		GetEventLoop().Break();
	}
};

int main(int argc, char **argv)
try {
	if (argc != 2)
		throw "Usage: CoLua FILE.lua";

	const char *path = argv[1];

	Instance instance;
	auto &event_loop = instance.event_loop;

	const auto L = luaL_newstate();
	AtScopeExit(L) { lua_close(L); };

	luaL_openlibs(L);

	InitEvent(L, event_loop);
	InitPg(L, event_loop);

	Thread thread(L, event_loop, path);

	event_loop.Run();

	thread.CheckRethrow();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
