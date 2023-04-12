// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoAwaitable.hxx"
#include "CoCancel.hxx"
#include "Thread.hxx"

namespace Lua {

CoAwaitable::CoAwaitable(Thread &_thread, lua_State *thread_L, int narg)
	:thread(_thread)
{
	SetResumeListener(thread_L, *this);
	Resume(thread_L, narg);
}

CoAwaitable::~CoAwaitable() noexcept
{
	if (ready) {
		assert(UnsetResumeListener(thread.GetMainState()) == nullptr);
		return;
	}

	const auto main_L = thread.GetMainState();
	const ScopeCheckStack check_main_stack{main_L};

	thread.Dispose(main_L, [](auto *L){
		if (UnsetResumeListener(L) != nullptr)
			CoCancel(L);
	});
}

void
CoAwaitable::OnLuaFinished(lua_State *) noexcept
{
	assert(!ready);
	assert(!error);

	ready = true;

	if (continuation)
		continuation.resume();
}

void
CoAwaitable::OnLuaError(lua_State *, std::exception_ptr e) noexcept
{
	assert(!ready);
	assert(!error);

	ready = true;
	error = std::move(e);

	if (continuation)
		continuation.resume();
}

} // namespace Lua
