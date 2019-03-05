/*
 * Copyright 2007-2018 Content Management AG
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

#ifndef NDEBUG
#include <stdio.h>
#endif

inline bool
EventLoop::TimerCompare::operator()(const TimerEvent &a,
				    const TimerEvent &b) const noexcept
{
	return a.due < b.due;
}

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() noexcept
{
#ifndef NDEBUG
	for (const auto &i : timers)
		fprintf(stderr, "EventLoop[%p]::~EventLoop() timer=%p\n",
			(const void *)this, (const void *)&i);
#endif

	assert(timers.empty());
	assert(defer.empty());
	assert(sockets.empty());
}

void
EventLoop::Reinit() noexcept
{
	steady_clock_cache.flush();
	system_clock_cache.flush();
	received_events.clear();

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
	for (auto &i : received_events)
		if (i.data.ptr == &event)
			i.events = 0;

	if (!epoll.Remove(fd))
		return false;

	sockets.erase(sockets.iterator_to(event));
	return true;
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

		timers.erase(i);

		t.Run();
	}

	return Event::Duration(-1);
}

void
EventLoop::Defer(DeferEvent &e) noexcept
{
	defer.push_front(e);
}

bool
EventLoop::RunDeferred() noexcept
{
	while (!defer.empty())
		defer.pop_front_and_dispose([](DeferEvent *e){
				e->OnDeferred();
			});

	return true;
}

/**
 * Convert the given timeout specification to a milliseconds integer,
 * to be used by functions like poll() and epoll_wait().  Any negative
 * value (= never times out) is translated to the magic value -1.
 */
static constexpr int
ExportTimeoutMS(Event::Duration timeout)
{
	return timeout >= timeout.zero()
		? int(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count())
		: -1;
}

bool
EventLoop::Loop(int flags) noexcept
{
	assert(received_events.empty());

	steady_clock_cache.flush();
	system_clock_cache.flush();

	quit = false;

	const bool once = flags & EVLOOP_ONCE;
	const bool nonblock = flags & EVLOOP_NONBLOCK;

	do {
		again = false;

		/* invoke timers */

		auto timeout = HandleTimers();
		if (quit)
			break;

		RunDeferred();
		if (again)
			/* re-evaluate timers because one of the
			   DeferEvents may have added a new timeout */
			continue;

		/* wait for new event */

		if (nonblock)
			timeout = timeout.zero();

		if (IsEmpty())
			return false;

		int ret = epoll.Wait(received_events.raw(),
				     received_events.capacity(),
				     ExportTimeoutMS(timeout));
		received_events.resize(std::max(0, ret));

		if (received_events.empty() && nonblock)
			quit = true;

		steady_clock_cache.flush();
		system_clock_cache.flush();

		/* invoke sockets */
		for (auto &i : received_events) {
			if (i.events == 0)
				continue;

			if (quit)
				break;

			auto &socket_event = *(SocketEvent *)i.data.ptr;
			socket_event.Dispatch(i.events);
		}

		received_events.clear();

		RunPost();
	} while (!quit && !once);

	return true;
}
