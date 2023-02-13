// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
