// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lua/RunFile.hxx"
#include "lua/Util.hxx"
#include "lua/Error.hxx"
#include "lua/Resume.hxx"
#include "lua/State.hxx"
#include "lua/json/ToJson.hxx"
#include "lua/event/Init.hxx"
#include "lua/pg/Init.hxx"
#include "lua/sodium/Init.hxx"
#include "event/Loop.hxx"
#include "event/DeferEvent.hxx"
#include "event/ShutdownListener.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveList.hxx"
#include "util/PrintException.hxx"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <forward_list>

#include <stdio.h>
#include <stdlib.h>

using namespace Lua;

struct Instance;

class Thread final : ResumeListener, public IntrusiveListHook<> {
	Instance &instance;

	lua_State *const L;

	DeferEvent start_event;

	const char *const path;

public:
	Thread(Instance &_instance,
	       lua_State *_L, EventLoop &event_loop,
	       const char *_path) noexcept
		:instance(_instance),
		 L(lua_newthread(_L)),
		 start_event(event_loop, BIND_THIS_METHOD(Start)),
		 path(_path)
	{
		SetResumeListener(L, *this);

		/* pop the lua_newthread() (a reference to it is held
		   by SetResumeListener()) */
		lua_pop(_L, 1);

		start_event.Schedule();
	}

	~Thread() noexcept {
		UnsetResumeListener(L);
	}

	auto &GetEventLoop() const noexcept {
		return start_event.GetEventLoop();
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

	void OnLuaFinished(lua_State *) noexcept override;
	void OnLuaError(lua_State *, std::exception_ptr &&e) noexcept override;
};

struct Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	const Lua::State state{luaL_newstate()};

	IntrusiveList<Thread> threads;
	std::forward_list<std::exception_ptr> errors;

	Instance() noexcept
	{
		shutdown_listener.Enable();
	}

	~Instance() noexcept {
		threads.clear_and_dispose(DeleteDisposer{});
	}

	void OnShutdown() noexcept {
		event_loop.Break();
	}

	void OnThreadFinished(Thread &thread) noexcept {
		threads.erase_and_dispose(threads.iterator_to(thread),
					  DeleteDisposer{});
		if (threads.empty())
			event_loop.Break();
	}
};

void
Thread::OnLuaFinished(lua_State *) noexcept
{
	instance.OnThreadFinished(*this);
}

void
Thread::OnLuaError(lua_State *, std::exception_ptr &&e) noexcept
{
	instance.errors.emplace_front(std::move(e));
	instance.OnThreadFinished(*this);
}

int main(int argc, char **argv)
try {
	if (argc < 2)
		throw "Usage: CoLua FILE.lua [FILE2.lua...]";

	Instance instance;
	auto &event_loop = instance.event_loop;

	lua_State *const L = instance.state.get();

	luaL_openlibs(L);

	InitResume(L);
	InitEvent(L, event_loop);
	InitPg(L, event_loop);
	InitSodium(L);
	InitToJson(L);

	for (int i = 1; i < argc; ++i)
		instance.threads.push_back(*new Thread(instance, L, event_loop, argv[i]));

	event_loop.Run();

	if (!instance.errors.empty()) {
		for (auto &i : instance.errors)
			PrintException(std::move(i));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
