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

#pragma once

#include "Chrono.hxx"
#include "util/BindMethod.hxx"
#include "util/Compiler.h"

#include <boost/intrusive/set_hook.hpp>

struct timeval;
class EventLoop;

/**
 * Invoke an event callback after a certain amount of time.
 */
class TimerEvent final
	: public boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
{
	friend class EventLoop;

	EventLoop &loop;

	const BoundMethod<void()> callback;

	/**
	 * When is this timer due?  This is only valid if IsPending()
	 * returns true.
	 */
	Event::Clock::time_point due;

public:
	TimerEvent(EventLoop &_loop, BoundMethod<void()> _callback) noexcept
		:loop(_loop), callback(_callback) {}

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	bool IsPending() const noexcept {
		return is_linked();
	}

	void Schedule(Event::Duration d) noexcept;

	/**
	 * Like Schedule(), but is a no-op if there is a due time
	 * earlier than the given one.
	 */
	void ScheduleEarlier(Event::Duration d) noexcept;

	void Cancel() noexcept {
		unlink();
	}

	/**
	 * Deprecated compatibility wrapper.  Use Schedule() instead.
	 */
	void Add(const struct timeval &tv) noexcept;

private:
	void Run() noexcept {
		callback();
	}
};
