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

#ifndef EVENT_BASE_HXX
#define EVENT_BASE_HXX

#include "TimerEvent.hxx"
#include "DeferEvent.hxx"
#include "SocketEvent.hxx"
#include "system/EpollFD.hxx"
#include "util/BindMethod.hxx"
#include "util/StaticArray.hxx"

#include <boost/intrusive/set.hpp>
#include <boost/intrusive/list.hpp>

#include <assert.h>

class SocketEvent;

/**
 * A non-blocking I/O event loop.
 */
class EventLoop {
	EpollFD epoll;

	struct TimerCompare {
		constexpr bool operator()(const TimerEvent &a,
					  const TimerEvent &b) const noexcept {
			return a.due < b.due;
		}
	};

	using TimerSet =
		boost::intrusive::multiset<TimerEvent,
					   boost::intrusive::member_hook<TimerEvent,
									 TimerEvent::TimerSetHook,
									 &TimerEvent::timer_set_hook>,
					   boost::intrusive::compare<TimerCompare>,
					   boost::intrusive::constant_time_size<false>>;
	TimerSet timers;

	boost::intrusive::list<DeferEvent,
			       boost::intrusive::member_hook<DeferEvent,
							     DeferEvent::SiblingsHook,
							     &DeferEvent::siblings>,
			       boost::intrusive::constant_time_size<false>> defer;

	using SocketList =
		boost::intrusive::list<SocketEvent,
				       boost::intrusive::constant_time_size<false>>;
	SocketList sockets;

#ifndef NDEBUG
	typedef BoundMethod<void() noexcept> PostCallback;
	PostCallback post_callback = nullptr;
#endif

	bool quit;

	/**
	 * True when the object has been modified and another check is
	 * necessary before going to sleep via PollGroup::ReadEvents().
	 */
	bool again;

	StaticArray<struct epoll_event, 32> received_events;

	static constexpr int EVLOOP_ONCE = 0x1;
	static constexpr int EVLOOP_NONBLOCK = 0x2;

public:
	EventLoop() = default;
	~EventLoop() noexcept;

	EventLoop(const EventLoop &other) = delete;
	EventLoop &operator=(const EventLoop &other) = delete;

	void Reinit() noexcept;

#ifndef NDEBUG
	/**
	 * Set a callback function which will be invoked each time an even
	 * has been handled.  This is debug-only and may be used to inject
	 * regular debug checks.
	 */
	void SetPostCallback(PostCallback new_value) noexcept {
		post_callback = new_value;
	}
#endif

	void Dispatch() noexcept {
		Loop(0);
	}

	bool LoopNonBlock() noexcept {
		return Loop(EVLOOP_NONBLOCK);
	}

	bool LoopOnce() noexcept {
		return Loop(EVLOOP_ONCE);
	}

	bool LoopOnceNonBlock() noexcept {
		return Loop(EVLOOP_ONCE|EVLOOP_NONBLOCK);
	}

	void Break() noexcept {
		quit = true;
	}

	bool IsEmpty() const noexcept {
		return timers.empty() && defer.empty() && sockets.empty();
	}

	bool AddFD(int fd, unsigned events, SocketEvent &event) noexcept {
		assert(events != 0);

		if (!epoll.Add(fd, events, &event))
			return false;

		sockets.push_back(event);
		return true;
	}

	bool ModifyFD(int fd, unsigned events, SocketEvent &event) noexcept {
		assert(events != 0);

		return epoll.Modify(fd, events, &event);
	}

	bool RemoveFD(int fd, SocketEvent &event) noexcept {
		for (auto &i : received_events)
			if (i.data.ptr == &event)
				i.events = 0;

		if (!epoll.Remove(fd))
			return false;

		sockets.erase(sockets.iterator_to(event));
		return true;
	}

	void AddTimer(TimerEvent &t,
		      std::chrono::steady_clock::duration d) noexcept;
	void CancelTimer(TimerEvent &t) noexcept;

	void Defer(DeferEvent &e) noexcept;
	void CancelDefer(DeferEvent &e) noexcept;

private:
	/**
	 * @return false if there are no registered events
	 */
	bool Loop(int flags) noexcept;

	bool RunDeferred() noexcept;

	/**
	 * Invoke all expired #TimerEvent instances and return the
	 * duration until the next timer expires.  Returns a negative
	 * duration if there is no timeout.
	 */
	std::chrono::steady_clock::duration HandleTimers() noexcept;

	bool RunPost() noexcept {
#ifndef NDEBUG
		if (post_callback)
			post_callback();
#endif
		return true;
	}
};

#endif
