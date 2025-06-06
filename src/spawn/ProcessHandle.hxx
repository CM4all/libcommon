// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class SpawnCompletionHandler;
class ExitListener;

/**
 * Handle to a child process spawned by
 * SpawnService::SpawnChildProcess().  It is meant to be wrapped in a
 * std::unique_ptr<>.
 */
class ChildProcessHandle {
public:
	/**
	 * Send SIGTERM to the child process and unregister it.
	 */
	virtual ~ChildProcessHandle() noexcept = default;

	virtual void SetCompletionHandler(SpawnCompletionHandler &handler) noexcept = 0;
	virtual void SetExitListener(ExitListener &listener) noexcept = 0;

	/**
	 * Send a signal to a child process and unregister it.
	 */
	virtual void Kill(int signo) noexcept = 0;
};
