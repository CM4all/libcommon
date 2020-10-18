/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "Chrono.hxx"
#include "system/EpollFD.hxx"
#include "time/ClockCache.hxx"
#include "util/IntrusiveList.hxx"

#ifndef NDEBUG
#include "util/BindMethod.hxx"
#endif

#include <boost/intrusive/set.hpp>

class TimerEvent;
class DeferEvent;
class SocketEvent;

/**
 * A non-blocking I/O event loop.
 */
class EventLoop {
	EpollFD epoll;

	struct TimerCompare {
		constexpr bool operator()(const TimerEvent &a,
					  const TimerEvent &b) const noexcept;
	};

	using TimerSet =
		boost::intrusive::multiset<TimerEvent,
					   boost::intrusive::base_hook<boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>>,
					   boost::intrusive::compare<TimerCompare>,
					   boost::intrusive::constant_time_size<false>>;
	TimerSet timers;

	using DeferList = IntrusiveList<DeferEvent>;

	DeferList defer;

	/**
	 * This is like #defer, but gets invoked when the loop is idle.
	 */
	DeferList idle;

	using SocketList = IntrusiveList<SocketEvent>;

	/**
	 * A list of scheduled #SocketEvent instances, without those
	 * which are ready (these are in #ready_sockets).
	 */
	SocketList sockets;

	/**
	 * A list of #SocketEvent instances which have non-zero
	 * "ready" flags and need to be dispatched.
	 */
	SocketList ready_sockets;

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

	/**
	 * This flag keeps track whether a TimerEvent or a DeferEvent
	 * has been invoked.  This is used to implement #EVLOOP_ONCE,
	 * to avoid calling epoll_wait() after something has been done
	 * already.
	 */
	bool invoked;

	ClockCache<std::chrono::steady_clock> steady_clock_cache;
	ClockCache<std::chrono::system_clock> system_clock_cache;

	static constexpr int EVLOOP_ONCE = 0x1;
	static constexpr int EVLOOP_NONBLOCK = 0x2;

public:
	EventLoop();
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
		return timers.empty() && defer.empty() && idle.empty() &&
			sockets.empty() && ready_sockets.empty();
	}

	bool AddFD(int fd, unsigned events, SocketEvent &event) noexcept;
	bool ModifyFD(int fd, unsigned events, SocketEvent &event) noexcept;
	bool RemoveFD(int fd, SocketEvent &event) noexcept;

	/**
	 * Remove the given #SocketEvent after the file descriptor
	 * has been closed.  This is like RemoveFD(), but does not
	 * attempt to use #EPOLL_CTL_DEL.
	 */
	void AbandonFD(SocketEvent &event) noexcept;

	void AddTimer(TimerEvent &t, Event::Duration d) noexcept;

	void Defer(DeferEvent &e) noexcept;
	void AddIdle(DeferEvent &e) noexcept;

	const auto &GetSteadyClockCache() const noexcept {
		return steady_clock_cache;
	}

	const auto &GetSystemClockCache() const noexcept {
		return system_clock_cache;
	}

	/**
	 * Caching wrapper for std::chrono::steady_clock::now().  The
	 * real clock is queried at most once per event loop
	 * iteration, because it is assumed that the event loop runs
	 * for a negligible duration.
	 */
	gcc_pure
	const auto &SteadyNow() const noexcept {
		return steady_clock_cache.now();
	}

	/**
	 * Caching wrapper for std::chrono::system_clock::now().  The
	 * real clock is queried at most once per event loop
	 * iteration, because it is assumed that the event loop runs
	 * for a negligible duration.
	 */
	gcc_pure
	const auto &SystemNow() const noexcept {
		return system_clock_cache.now();
	}

private:
	/**
	 * @return false if there are no registered events
	 */
	bool Loop(int flags) noexcept;

	bool RunDeferred() noexcept;

	/**
	 * Invoke one "idle" #DeferEvent.
	 *
	 * @return false if there was no such event
	 */
	bool RunOneIdle() noexcept;

	/**
	 * Invoke all expired #TimerEvent instances and return the
	 * duration until the next timer expires.  Returns a negative
	 * duration if there is no timeout.
	 */
	Event::Duration HandleTimers() noexcept;

	/**
	 * Call epoll_wait() and pass all returned events to
	 * SocketEvent::SetReadyFlags().
	 *
	 * @return true if one or more sockets have become ready
	 */
	bool Wait(Event::Duration timeout) noexcept;

	bool RunPost() noexcept {
#ifndef NDEBUG
		if (post_callback)
			post_callback();
#endif
		return true;
	}
};

#endif
