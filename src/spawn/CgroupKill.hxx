/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "io/UniqueFileDescriptor.hxx"
#include "event/SocketEvent.hxx"
#include "event/DeferEvent.hxx"
#include "event/TimerEvent.hxx"

#include <exception>
#include <string>

struct CgroupState;

class CgroupKillHandler {
public:
	virtual void OnCgroupKill() noexcept = 0;
	virtual void OnCgroupKillError(std::exception_ptr error) noexcept = 0;
};

/**
 * Kill all remaining processes in the given cgroup (first SIGTERM and
 * then SIGKILL).  Waits for some time for the cgroup to become empty.
 *
 * Note that this class does not traverse child cgroups.  If there are
 * populated child cgroups, this class will report an error.
 */
class CgroupKill {
	CgroupKillHandler &handler;

	UniqueFileDescriptor inotify_fd;
	SocketEvent inotify_event;

	UniqueFileDescriptor cgroup_events_fd, cgroup_procs_fd;

	DeferEvent send_term_event;
	TimerEvent send_kill_event;
	TimerEvent timeout_event;

	CgroupKill(EventLoop &event_loop,
		   const std::string &cgroup_path,
		   UniqueFileDescriptor &&cgroup_fd,
		   CgroupKillHandler &_handler);

public:
	/**
	 * Throws on error.
	 */
	CgroupKill(EventLoop &event_loop, const CgroupState &state,
		   const char *name, const char *session,
		   CgroupKillHandler &_handler);

private:
	void Disable() noexcept {
		inotify_event.Cancel();
		send_term_event.Cancel();
		send_kill_event.Cancel();
		timeout_event.Cancel();
	}

	bool CheckPopulated() noexcept;

	void OnInotifyEvent(unsigned events) noexcept;
	void OnSendTerm() noexcept;
	void OnSendKill() noexcept;
	void OnTimeout() noexcept;
};
