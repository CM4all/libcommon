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

#ifndef EVENT_HXX
#define EVENT_HXX

#include "Loop.hxx"

#include "util/Compiler.h"

#include <event.h>

/**
 * Wrapper for a struct event.
 */
class Event {
	struct event event;

public:
	Event() = default;

	Event(EventLoop &loop, evutil_socket_t fd, short mask,
	      event_callback_fn callback, void *ctx) {
		Set(loop, fd, mask, callback, ctx);
	}

#ifndef NDEBUG
	~Event() {
		event_debug_unassign(&event);
	}
#endif

	Event(const Event &other) = delete;
	Event &operator=(const Event &other) = delete;

	/**
	 * Check if the event was initialized.  Calling this method is
	 * only legal if it really was initialized or if the memory is
	 * zeroed (e.g. an uninitialized global/static variable).
	 */
	gcc_pure
	bool IsInitialized() const {
		return event_initialized(&event);
	}

	gcc_pure
	evutil_socket_t GetFd() const {
		return event_get_fd(&event);
	}

	gcc_pure
	short GetEvents() const {
		return event_get_events(&event);
	}

	gcc_pure
	event_callback_fn GetCallback() const {
		return event_get_callback(&event);
	}

	gcc_pure
	void *GetCallbackArg() const {
		return event_get_callback_arg(&event);
	}

	void Set(EventLoop &loop, evutil_socket_t fd, short mask,
		 event_callback_fn callback, void *ctx) {
		::event_assign(&event, loop.Get(), fd, mask, callback, ctx);
	}

	bool Add(const struct timeval *timeout=nullptr) {
		return ::event_add(&event, timeout) == 0;
	}

	bool Add(const struct timeval &timeout) {
		return Add(&timeout);
	}

	void SetTimer(event_callback_fn callback, void *ctx) {
		::evtimer_set(&event, callback, ctx);
	}

	void SetSignal(int sig, event_callback_fn callback, void *ctx) {
		::evsignal_set(&event, sig, callback, ctx);
	}

	void Delete() {
		::event_del(&event);
	}

	gcc_pure
	bool IsPending(short events) const {
		return ::event_pending(&event, events, nullptr);
	}

	gcc_pure
	bool IsTimerPending() const {
		return IsPending(EV_TIMEOUT);
	}
};

#endif
