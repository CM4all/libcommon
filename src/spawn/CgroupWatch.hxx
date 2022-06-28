/*
 * Copyright 2017-2022 CM4all GmbH
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

#include "event/Chrono.hxx"
#include "event/InotifyEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

struct CgroupState;

class CgroupMemoryWatch final : InotifyHandler {
	UniqueFileDescriptor fd;

	InotifyEvent inotify;

	Event::TimePoint next_time;

	BoundMethod<void(uint64_t value) noexcept> callback;

public:
	/**
	 * Throws if the group memory usage file could not be opened.
	 */
	CgroupMemoryWatch(EventLoop &event_loop,
			  const CgroupState &state,
			  BoundMethod<void(uint64_t value) noexcept> _callback);

	auto &GetEventLoop() const noexcept {
		return inotify.GetEventLoop();
	}

private:
	/* virtual methods from class InotifyHandler */
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};
