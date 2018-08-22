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

#ifndef NDEBUG
#include <stdio.h>
#endif

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
	received_events.clear();

	epoll = {};

	for (auto &i : sockets) {
		assert(i.GetScheduledFlags() != 0);

		epoll.Add(i.GetSocket().Get(), i.GetScheduledFlags(), &i);
	}
}

void
EventLoop::AddTimer(TimerEvent &t, std::chrono::steady_clock::duration d) noexcept
{
	t.due = std::chrono::steady_clock::now() + d;
	timers.insert(t);
	again = true;
}

void
EventLoop::CancelTimer(TimerEvent &t) noexcept
{
	timers.erase(timers.iterator_to(t));
}

inline std::chrono::steady_clock::duration
EventLoop::HandleTimers() noexcept
{
	const auto now = std::chrono::steady_clock::now();

	std::chrono::steady_clock::duration timeout;

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

	return std::chrono::steady_clock::duration(-1);
}

void
EventLoop::Defer(DeferEvent &e) noexcept
{
	defer.push_front(e);
}

void
EventLoop::CancelDefer(DeferEvent &e) noexcept
{
	defer.erase(defer.iterator_to(e));
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
ExportTimeoutMS(std::chrono::steady_clock::duration timeout)
{
	return timeout >= timeout.zero()
		? int(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count())
		: -1;
}

bool
EventLoop::Loop(int flags) noexcept
{
	assert(received_events.empty());

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
