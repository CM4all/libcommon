/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "Loop.hxx"
#include "DeferEvent.hxx"
#include "SocketEvent.hxx"

#include <array>

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() noexcept
{
	assert(defer.empty());
	assert(idle.empty());
	assert(sockets.empty());
	assert(ready_sockets.empty());
}

bool
EventLoop::AddFD(int fd, unsigned events, SocketEvent &event) noexcept
{
	assert(events != 0);

	if (!poll_backend.Add(fd, events, &event))
		return false;

	sockets.push_back(event);
	return true;
}

bool
EventLoop::ModifyFD(int fd, unsigned events, SocketEvent &event) noexcept
{
	assert(events != 0);

	return poll_backend.Modify(fd, events, &event);
}

bool
EventLoop::RemoveFD(int fd, SocketEvent &event) noexcept
{
	event.unlink();
	return poll_backend.Remove(fd);
}

void
EventLoop::AbandonFD(SocketEvent &event) noexcept
{
	event.unlink();
}

void
EventLoop::Insert(CoarseTimerEvent &t) noexcept
{
	coarse_timers.Insert(t, SteadyNow());
	again = true;
}

#ifndef NO_FINE_TIMER_EVENT

void
EventLoop::Insert(FineTimerEvent &t) noexcept
{
	timers.Insert(t);
	again = true;
}

#endif // NO_FINE_TIMER_EVENT

inline Event::Duration
EventLoop::HandleTimers() noexcept
{
	const auto now = SteadyNow();

#ifndef NO_FINE_TIMER_EVENT
	auto fine_timeout = timers.Run(now);
#else
	const Event::Duration fine_timeout{-1};
#endif // NO_FINE_TIMER_EVENT
	auto coarse_timeout = coarse_timers.Run(now);

	return fine_timeout.count() < 0 ||
		(coarse_timeout.count() >= 0 && coarse_timeout < fine_timeout)
		? coarse_timeout
		: fine_timeout;
}

void
EventLoop::AddDefer(DeferEvent &e) noexcept
{
	defer.push_back(e);
}

void
EventLoop::AddIdle(DeferEvent &e) noexcept
{
	idle.push_back(e);
}

void
EventLoop::RunDeferred() noexcept
{
	while (!defer.empty() && !quit) {
		defer.pop_front_and_dispose([](DeferEvent *e){
			e->Run();
		});
	}
}

bool
EventLoop::RunOneIdle() noexcept
{
	if (idle.empty())
		return false;

	idle.pop_front_and_dispose([](DeferEvent *e){
		e->Run();
	});

	return true;
}

template<class ToDuration, class Rep, class Period>
static constexpr ToDuration
duration_cast_round_up(std::chrono::duration<Rep, Period> d) noexcept
{
	using FromDuration = decltype(d);
	constexpr auto one = std::chrono::duration_cast<FromDuration>(ToDuration(1));
	constexpr auto round_add = one > one.zero()
		? one - FromDuration(1)
		: one.zero();
	return std::chrono::duration_cast<ToDuration>(d + round_add);
}

/**
 * Convert the given timeout specification to a milliseconds integer,
 * to be used by functions like poll() and epoll_wait().  Any negative
 * value (= never times out) is translated to the magic value -1.
 */
static constexpr int
ExportTimeoutMS(Event::Duration timeout) noexcept
{
	return timeout >= timeout.zero()
		? int(duration_cast_round_up<std::chrono::milliseconds>(timeout).count())
		: -1;
}

inline bool
EventLoop::Wait(Event::Duration timeout) noexcept
{
	std::array<struct epoll_event, 256> received_events;
	int ret = poll_backend.Wait(received_events.data(),
				    received_events.size(),
				    ExportTimeoutMS(timeout));
	for (int i = 0; i < ret; ++i) {
		const auto &e = received_events[i];
		auto &socket_event = *(SocketEvent *)e.data.ptr;
		socket_event.SetReadyFlags(e.events);

		/* move from "sockets" to "ready_sockets" */
		socket_event.unlink();
		ready_sockets.push_back(socket_event);
	}

	return ret > 0;
}

void
EventLoop::Run() noexcept
{
	FlushClockCaches();

	quit = false;

	do {
		again = false;

		/* invoke timers */

		const auto timeout = HandleTimers();
		if (quit)
			break;

		RunDeferred();
		if (quit)
			break;

		if (RunOneIdle())
			/* check for other new events after each
			   "idle" invocation to ensure that the other
			   "idle" events are really invoked at the
			   very end */
			continue;

		if (again)
			/* re-evaluate timers because one of the
			   DeferEvents may have added a new timeout */
			continue;

		/* wait for new event */

		if (IsEmpty())
			return;

		if (ready_sockets.empty()) {
			Wait(timeout);
			FlushClockCaches();
		}

		/* invoke sockets */
		while (!ready_sockets.empty() && !quit) {
			auto &socket_event = ready_sockets.front();

			/* move from "ready_sockets" back to "sockets" */
			socket_event.unlink();
			sockets.push_back(socket_event);

			socket_event.Dispatch();
		}

		RunPost();
	} while (!quit);
}
