// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Chrono.hxx"
#include "TimerWheel.hxx"
#include "system/EpollFD.hxx"
#include "time/ClockCache.hxx"
#include "util/IntrusiveList.hxx"
#include "event/config.h"

#ifndef NO_FINE_TIMER_EVENT
#include "TimerList.hxx"
#endif // NO_FINE_TIMER_EVENT

#ifdef HAVE_THREADED_EVENT_LOOP
#include "WakeFD.hxx"
#include "SocketEvent.hxx"
#include "thread/Id.hxx"
#include "thread/Mutex.hxx"
#endif

#ifndef NDEBUG
#include "util/BindMethod.hxx"
#endif

#ifdef HAVE_URING
#include <memory>
struct io_uring_params;
namespace Uring { class Queue; class Manager; }
#endif

#include <cassert>

class DeferEvent;
class SocketEvent;

/**
 * An event loop that polls for events on file/socket descriptors.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs it, except where explicitly documented as
 * thread-safe.
 *
 * @see SocketEvent, TimerEvent, DeferEvent
 */
class EventLoop final
{
	EpollFD poll_backend;

#ifdef HAVE_THREADED_EVENT_LOOP
	WakeFD wake_fd;
	SocketEvent wake_event{*this, BIND_THIS_METHOD(OnSocketReady), wake_fd.GetSocket()};
#endif

	TimerWheel coarse_timers;

#ifndef NO_FINE_TIMER_EVENT
	TimerList timers;
#endif // NO_FINE_TIMER_EVENT

	using DeferList = IntrusiveList<DeferEvent>;

	DeferList defer;

	/**
	 * This is like #defer, but gets invoked when the loop is idle.
	 */
	DeferList idle;

	/**
	 * This is like #idle, but gets invoked after the next
	 * epoll_wait() call.
	 */
	DeferList next;

#ifdef HAVE_THREADED_EVENT_LOOP
	Mutex mutex;

	using InjectList = IntrusiveList<InjectEvent>;
	InjectList inject;
#endif

	using SocketList = IntrusiveList<SocketEvent>;

	/**
	 * A list of scheduled #SocketEvent instances, without those
	 * which are ready (these are in #ready_sockets).
	 */
	SocketList sockets;

	/**
	 * A list of #SocketEvent instances which have a non-zero
	 * "ready_flags" field and need to be dispatched.
	 */
	SocketList ready_sockets;

#ifdef HAVE_URING
	std::unique_ptr<Uring::Manager> uring;
#endif

#ifndef NDEBUG
	using PostCallback = BoundMethod<void() noexcept>;
	PostCallback post_callback = nullptr;
#endif

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * A reference to the thread that is currently inside Run().
	 */
	ThreadId thread = ThreadId::Null();

	/**
	 * Is this #EventLoop alive, i.e. can events be scheduled?
	 * This is used by BlockingCall() to determine whether
	 * schedule in the #EventThread or to call directly (if
	 * there's no #EventThread yet/anymore).
	 */
	bool alive;
#endif

	bool quit;

	/**
	 * True when the object has been modified and another check is
	 * necessary before going to sleep via PollGroup::ReadEvents().
	 */
	bool again;

#ifdef HAVE_THREADED_EVENT_LOOP
	bool quit_injected = false;

	/**
	 * True when handling callbacks, false when waiting for I/O or
	 * timeout.
	 *
	 * Protected with #mutex.
	 */
	bool busy = true;
#endif

	ClockCache<std::chrono::steady_clock> steady_clock_cache;
	ClockCache<std::chrono::system_clock> system_clock_cache;

public:
	/**
	 * Throws on error.
	 */
#ifdef HAVE_THREADED_EVENT_LOOP
	explicit EventLoop(ThreadId _thread);

	EventLoop():EventLoop(ThreadId::GetCurrent()) {}
#else
	EventLoop();
#endif
	~EventLoop() noexcept;

	EventLoop(const EventLoop &other) = delete;
	EventLoop &operator=(const EventLoop &other) = delete;

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
	[[gnu::pure]]
	const auto &SteadyNow() const noexcept {
#ifdef HAVE_THREADED_EVENT_LOOP
		assert(IsInside());
#endif

		return steady_clock_cache.now();
	}

	/**
	 * Caching wrapper for std::chrono::system_clock::now().  The
	 * real clock is queried at most once per event loop
	 * iteration, because it is assumed that the event loop runs
	 * for a negligible duration.
	 */
	[[gnu::pure]]
	const auto &SystemNow() const noexcept {
#ifdef HAVE_THREADED_EVENT_LOOP
		assert(IsInside());
#endif

		return system_clock_cache.now();
	}

	void FlushClockCaches() noexcept {
		steady_clock_cache.flush();
		system_clock_cache.flush();
	}

	void SetVolatile() noexcept;

#ifdef HAVE_URING
	/**
	 * Try to enable io_uring support.  If this method succeeds,
	 * GetUring() can be used to obtain a pointer to the queue
	 * instance.
	 *
	 * Throws on error.
	 */
	void EnableUring(unsigned entries, unsigned flags);
	void EnableUring(unsigned entries, struct io_uring_params &params);

	/**
	 * Returns a pointer to the io_uring queue instance or nullptr
	 * if io_uring support is not available (or was not enabled
	 * using EnableUring()).
	 */
	[[nodiscard]] [[gnu::const]]
	Uring::Queue *GetUring() noexcept;
#endif

	/**
	 * Stop execution of this #EventLoop at the next chance.
	 *
	 * This method is not thread-safe.  For stopping the
	 * #EventLoop from within another thread, use InjectBreak().
	 */
	void Break() noexcept {
		quit = true;
	}

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * Like Break(), but thread-safe.  It is also non-blocking:
	 * after returning, it is not guaranteed that the EventLoop
	 * has really stopped.
	 */
	void InjectBreak() noexcept {
		{
			const std::scoped_lock lock{mutex};
			quit_injected = true;
		}

		wake_fd.Write();
	}
#endif // HAVE_THREADED_EVENT_LOOP

	bool IsEmpty() const noexcept {
		return coarse_timers.IsEmpty() &&
#ifndef NO_FINE_TIMER_EVENT
			timers.IsEmpty() &&
#endif // NO_FINE_TIMER_EVENT
			defer.empty() && idle.empty() && next.empty() &&
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

	void Insert(CoarseTimerEvent &t) noexcept;

#ifndef NO_FINE_TIMER_EVENT
	void Insert(FineTimerEvent &t) noexcept;
#endif // NO_FINE_TIMER_EVENT

	/**
	 * Schedule a call to DeferEvent::RunDeferred().
	 */
	void AddDefer(DeferEvent &e) noexcept;
	void AddIdle(DeferEvent &e) noexcept;
	void AddNext(DeferEvent &e) noexcept;

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * Schedule a call to the InjectEvent.
	 *
	 * This method is thread-safe.
	 */
	void AddInject(InjectEvent &d) noexcept;

	/**
	 * Cancel a pending call to the InjectEvent.
	 * However after returning, the call may still be running.
	 *
	 * This method is thread-safe.
	 */
	void RemoveInject(InjectEvent &d) noexcept;
#endif

	/**
	 * The main function of this class.  It will loop until
	 * Break() gets called.  Can be called only once.
	 */
	void Run() noexcept;

private:
	void RunDeferred() noexcept;

	/**
	 * Invoke one "idle" #DeferEvent.
	 *
	 * @return false if there was no such event
	 */
	bool RunOneIdle() noexcept;

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * Invoke all pending InjectEvents.
	 *
	 * Caller must lock the mutex.
	 */
	void HandleInject() noexcept;
#endif

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

#ifdef HAVE_THREADED_EVENT_LOOP
	void OnSocketReady(unsigned flags) noexcept;
#endif

	void RunPost() noexcept {
#ifndef NDEBUG
		if (post_callback)
			post_callback();
#endif
	}

public:
#ifdef HAVE_THREADED_EVENT_LOOP
	void SetAlive(bool _alive) noexcept {
		alive = _alive;
	}

	bool IsAlive() const noexcept {
		return alive;
	}
#endif

	/**
	 * Are we currently running inside this EventLoop's thread?
	 */
	[[gnu::pure]]
	bool IsInside() const noexcept {
#ifdef HAVE_THREADED_EVENT_LOOP
		return thread.IsInside();
#else
		return true;
#endif
	}
};
