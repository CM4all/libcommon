// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/IntrusiveList.hxx"

#include <memory>

class ExitListener;
class PidfdEvent;

/**
 * Manage child processes.
 */
class ChildProcessRegistry {
	class KilledChildProcess;
	IntrusiveList<KilledChildProcess> killed_list;

public:
	ChildProcessRegistry() noexcept;
	~ChildProcessRegistry() noexcept;

	/**
	 * Send a signal to the given child process and expect it to
	 * exit.  If it does not, a timer will trigger sending SIGKILL
	 * after some time.
	 */
	void Kill(std::unique_ptr<PidfdEvent> pidfd,
		  int signo) noexcept;
};
