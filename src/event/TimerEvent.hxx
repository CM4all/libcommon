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

#ifndef BENG_PROXY_TIMER_EVENT_HXX
#define BENG_PROXY_TIMER_EVENT_HXX

#include "Event.hxx"
#include "util/BindMethod.hxx"

/**
 * Invoke an event callback after a certain amount of time.
 */
class TimerEvent {
	Event event;

	const BoundMethod<void()> callback;

public:
	TimerEvent(EventLoop &loop, BoundMethod<void()> _callback)
		:event(loop, -1, 0, Callback, this), callback(_callback) {}

	bool IsPending() const {
		return event.IsTimerPending();
	}

	void Add(const struct timeval &tv) {
		event.Add(tv);
	}

	void Cancel() {
		event.Delete();
	}

private:
	static void Callback(gcc_unused evutil_socket_t fd,
			     gcc_unused short events,
			     void *ctx) {
		auto &event = *(TimerEvent *)ctx;
		event.callback();
	}
};

#endif
