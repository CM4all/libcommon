/*
 * Copyright 2020 CM4all GmbH
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

#pragma once

#include "Compat.hxx"
#include "event/TimerEvent.hxx"

namespace Co {

/**
 * Put a coroutine to sleep by suspending it on `co_await` and
 * resuming it as soon as the #TimerEvent fires.
 */
class Sleep final {
	TimerEvent event;

	std::coroutine_handle<> continuation;

public:
	Sleep(EventLoop &event_loop, Event::Duration d) noexcept
		:event(event_loop, BIND_THIS_METHOD(OnTimer))
	{
		event.Schedule(d);
	}

	auto operator co_await() noexcept {
		struct Awaitable final {
			Sleep &sleep;

			bool await_ready() const noexcept {
				return !sleep.event.IsPending();
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
				sleep.continuation = continuation;
				return std::noop_coroutine();
			}

			void await_resume() noexcept {
			}
		};

		return Awaitable{*this};
	}

private:
	void OnTimer() noexcept {
		continuation.resume();
	}
};

} // namespace Co

