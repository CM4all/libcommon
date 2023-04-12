// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoRunner.hxx"
#include "CoCancel.hxx"
#include "Resume.hxx"

namespace Lua {

lua_State *
CoRunner::CreateThread(ResumeListener &listener)
{
	const auto main_L = GetMainState();
	const ScopeCheckStack check_main_stack(main_L);

	/* create a new thread for the coroutine */
	const auto L = thread.Create(main_L);
	/* pop the new thread from the main stack */
	lua_pop(main_L, 1);

	SetResumeListener(L, listener);
	return L;
}

void
CoRunner::Cancel()
{
	const auto main_L = GetMainState();
	const ScopeCheckStack check_main_stack(main_L);

	thread.Dispose(main_L, [](auto *L){
		if (UnsetResumeListener(L) != nullptr)
			CoCancel(L);
	});
}

} // namespace Lua
