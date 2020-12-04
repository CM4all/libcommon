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

#pragma once

#include "SocketEvent.hxx"
#include "util/BindMethod.hxx"

#include <assert.h>
#include <signal.h>

/**
 * Listen for signals delivered to this process, and then invoke a
 * callback.
 *
 * After constructing an instance, call Add() to add signals to listen
 * on.  When done, call Enable().  After that, Add() must not be
 * called again.
 */
class SignalEvent {
	SocketEvent event;

	sigset_t mask;

	using Callback = BoundMethod<void(int) noexcept>;
	const Callback callback;

public:
	SignalEvent(EventLoop &loop, Callback _callback) noexcept;

	SignalEvent(EventLoop &loop, int signo, Callback _callback) noexcept
		:SignalEvent(loop, _callback) {
		Add(signo);
	}

	~SignalEvent() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	void Add(int signo) noexcept {
		assert(!IsDefined());

		sigaddset(&mask, signo);
	}

	void Enable();
	void Disable() noexcept;

private:
	void EventCallback(unsigned events) noexcept;
};
