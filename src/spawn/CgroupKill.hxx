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

#include "io/UniqueFileDescriptor.hxx"
#include "event/InotifyEvent.hxx"
#include "event/DeferEvent.hxx"
#include "event/CoarseTimerEvent.hxx"

#include <exception>

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
class CgroupKill final : InotifyHandler {
	CgroupKillHandler &handler;

	InotifyEvent inotify_event;

	UniqueFileDescriptor cgroup_events_fd, cgroup_procs_fd, cgroup_kill_fd;

	DeferEvent send_term_event;
	CoarseTimerEvent send_kill_event;
	CoarseTimerEvent timeout_event;

	CgroupKill(EventLoop &event_loop, const CgroupState &state,
		   FileDescriptor cgroup_fd,
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
		inotify_event.Disable();
		send_term_event.Cancel();
		send_kill_event.Cancel();
		timeout_event.Cancel();
	}

	bool CheckPopulated() noexcept;

	void OnSendTerm() noexcept;
	void OnSendKill() noexcept;
	void OnTimeout() noexcept;

	/* virtual methods from class InotifyHandler */
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};
