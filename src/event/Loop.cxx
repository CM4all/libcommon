/*
 * Copyright 2007-2021 CM4all GmbH
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
#include "TimerEvent.hxx"
#include "DeferEvent.hxx"
#include "SocketEvent.hxx"

#include <array>

constexpr bool
EventLoop::TimerCompare::operator()(const TimerEvent &a,
				    const TimerEvent &b) const noexcept
{
	return a.due < b.due;
}

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() noexcept
{
	assert(timers.empty());
	assert(defer.empty());
	assert(idle.empty());
	assert(sockets.empty());
	assert(ready_sockets.empty());
}

void
EventLoop::Reinit() noexcept
{
	steady_clock_cache.flush();
	system_clock_cache.flush();

	epoll = {};

	for (auto &i : sockets) {
		assert(i.GetScheduledFlags() != 0);

		epoll.Add(i.GetSocket().Get(), i.GetScheduledFlags(), &i);
	}
}

bool
EventLoop::AddFD(int fd, unsigned events, SocketEvent &event) noexcept
{
	assert(events != 0);

	if (!epoll.Add(fd, events, &event))
		return false;

	sockets.push_back(event);
	return true;
}

bool
EventLoop::ModifyFD(int fd, unsigned events, SocketEvent &event) noexcept
{
	assert(events != 0);

	return epoll.Modify(fd, events, &event);
}

bool
EventLoop::RemoveFD(int fd, SocketEvent &event) noexcept
{
	event.unlink();
	return epoll.Remove(fd);
}

void
EventLoop::AbandonFD(SocketEvent &event) noexcept
{
	event.unlink();
}

void
EventLoop::AddTimer(TimerEvent &t, Event::Duration d) noexcept
{
	t.due = SteadyNow() + d;
	timers.insert(t);
	again = true;
}

inline Event::Duration
EventLoop::HandleTimers() noexcept
{
	const auto now = SteadyNow();

	Event::Duration timeout;

	while (!quit) {
		auto i = timers.begin();
		if (i == timers.end())
			break;

		TimerEvent &t = *i;
		timeout = t.due - now;
		if (timeout > timeout.zero())
			return timeout;

		invoked = true;

		timers.erase(i);

		t.Run();
	}

	return Event::Duration(-1);
}

void
EventLoop::AddDefer(DeferEvent &e) noexcept
{
	defer.push_front(e);
}

void
EventLoop::AddIdle(DeferEvent &e) noexcept
{
	idle.push_front(e);
}

bool
EventLoop::RunDeferred() noexcept
{
	while (!defer.empty()) {
		invoked = true;
		defer.pop_front_and_dispose([](DeferEvent *e){
			e->Run();
		});
	}

	return true;
}

bool
EventLoop::RunOneIdle() noexcept
{
	if (idle.empty())
		return false;

	invoked = true;
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
	int ret = epoll.Wait(received_events.data(),
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

bool
EventLoop::Loop(int flags) noexcept
{
	steady_clock_cache.flush();
	system_clock_cache.flush();

	quit = false;

	const bool once = flags & EVLOOP_ONCE;
	const bool nonblock = flags & EVLOOP_NONBLOCK;

	do {
		again = false;
		invoked = false;

		/* invoke timers */

		auto timeout = HandleTimers();
		if (quit)
			break;

		RunDeferred();

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

		if (once && invoked)
			break;

		/* wait for new event */

		if (nonblock)
			timeout = timeout.zero();

		if (IsEmpty())
			return false;

		const bool ready = !ready_sockets.empty() || Wait(timeout);
		if (!ready && nonblock)
			quit = true;

		steady_clock_cache.flush();
		system_clock_cache.flush();

		/* invoke sockets */
		while (!ready_sockets.empty() && !quit) {
			auto &socket_event = ready_sockets.front();

			/* move from "ready_sockets" back to "sockets" */
			socket_event.unlink();
			sockets.push_back(socket_event);

			socket_event.Dispatch();
		}

		RunPost();
	} while (!quit && !once);

	return true;
}
