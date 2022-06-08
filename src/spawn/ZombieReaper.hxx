/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "event/SignalEvent.hxx"
#include "event/DeferEvent.hxx"

/**
 * A handler for SIGCHLD which reaps all child processes, but ignores
 * their exit status.  The waitpid() system call is invoked using
 * DeferEvent::ScheduleIdle(), to give all pidfd handlers a chance to
 * invoke waitid() before this class reaps everything.
 *
 * This class is intended to fix two problems: (1) grandchildren (for
 * which we have no pidfd) which are orphaned and need to be reaped by
 * us; and (2) reaping children whose pidfd was automatically closed
 * because spawning has failed, and waitid() was not called.
 */
class ZombieReaper {
	SignalEvent sigchld;

	DeferEvent defer_wait;

public:
	explicit ZombieReaper(EventLoop &event_loop);

	auto &GetEventLoop() const noexcept {
		return sigchld.GetEventLoop();
	}

	void Disable() noexcept {
		sigchld.Disable();
	}

private:
	void OnSigchld(int) noexcept {
		defer_wait.ScheduleIdle();
	}

	void DoReap() noexcept;
};
