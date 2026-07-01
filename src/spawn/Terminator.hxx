// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Stats.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>

class PidfdEvent;

/**
 * Manage child process termination.
 */
class ChildProcessTerminator {
	class KilledChildProcess;
	IntrusiveList<KilledChildProcess> killed_list;

	ChildProcessTerminatorStats stats{};

public:
	ChildProcessTerminator() noexcept;
	~ChildProcessTerminator() noexcept;

	const ChildProcessTerminatorStats &GetStats() const noexcept {
		return stats;
	}

	/**
	 * Send a signal to the given child process and expect it to
	 * exit.  If it does not, a timer will trigger sending SIGKILL
	 * after some time.
	 */
	void Kill(std::unique_ptr<PidfdEvent> pidfd,
		  int signo) noexcept;
};
