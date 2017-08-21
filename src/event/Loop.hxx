/*
 * Copyright 2007-2017 Content Management AG
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

#include "DeferEvent.hxx"
#include "util/BindMethod.hxx"

#include <event.h>

#include <assert.h>

/**
 * Wrapper for a struct event_base.
 */
class EventLoop {
	struct event_base *const event_base;

	static struct event_base *Create() {
#if 0
		/* TODO: disabled for now, because a libevent bug crashes the
		   spawner on event_reinit() */
#ifndef NDEBUG
		/* call event_enable_debug_mode() only once, before the first
		   event_init() call */
		static struct DebugMode {
			DebugMode() {
				event_enable_debug_mode();
			}
		} once_enable_debug_mode;
#endif
#endif

		return ::event_init();
	}

	boost::intrusive::list<DeferEvent,
			       boost::intrusive::member_hook<DeferEvent,
							     DeferEvent::SiblingsHook,
							     &DeferEvent::siblings>,
			       boost::intrusive::constant_time_size<false>> defer;

#ifndef NDEBUG
	typedef BoundMethod<void()> PostCallback;
	PostCallback post_callback = nullptr;
#endif

	bool quit;

public:
	EventLoop():event_base(Create()) {}

	~EventLoop() {
		assert(defer.empty());

		::event_base_free(event_base);
	}

	EventLoop(const EventLoop &other) = delete;
	EventLoop &operator=(const EventLoop &other) = delete;

	struct event_base *Get() {
		return event_base;
	}

	void Reinit() {
		event_reinit(event_base);
	}

#ifndef NDEBUG
	/**
	 * Set a callback function which will be invoked each time an even
	 * has been handled.  This is debug-only and may be used to inject
	 * regular debug checks.
	 */
	void SetPostCallback(PostCallback new_value) {
		post_callback = new_value;
	}
#endif

	void Dispatch() {
		quit = false;

		RunDeferred();
		while (Loop(EVLOOP_ONCE) && !quit) {
			RunDeferred();
			RunPost();
		}
	}

	bool LoopNonBlock() {
		return RunDeferred() && Loop(EVLOOP_NONBLOCK) &&
			RunDeferred() &&
			RunPost();
	}

	bool LoopOnce() {
		return RunDeferred() && Loop(EVLOOP_ONCE) &&
			RunDeferred() &&
			RunPost();
	}

	bool LoopOnceNonBlock() {
		return RunDeferred() && Loop(EVLOOP_ONCE|EVLOOP_NONBLOCK) &&
			RunDeferred() &&
			RunPost();
	}

	void Break() {
		quit = true;
		::event_base_loopbreak(event_base);
	}

	void DumpEvents(FILE *file) {
		event_base_dump_events(event_base, file);
	}

	void Defer(DeferEvent &e);
	void CancelDefer(DeferEvent &e);

private:
	bool Loop(int flags) {
		return ::event_base_loop(event_base, flags) == 0;
	}

	bool RunDeferred();

	bool RunPost() {
#ifndef NDEBUG
		if (post_callback)
			post_callback();
#endif
		return true;
	}
};

#endif
