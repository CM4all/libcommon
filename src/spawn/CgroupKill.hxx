// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
