// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/BindMethod.hxx"

#include <memory>

struct PreparedChildProcess;
class ChildProcessHandle;
class CancellablePointer;

/**
 * A service which can spawn new child processes according to a
 * #PreparedChildProcess instance.
 */
class SpawnService {
public:
	using EnqueueCallback = BoundMethod<void() noexcept>;

	/**
	 * Throws std::runtime_error on error.
	 *
	 * @return a process id
	 */
	[[nodiscard]]
	virtual std::unique_ptr<ChildProcessHandle> SpawnChildProcess(const char *name,
								      PreparedChildProcess &&params) = 0;

	/**
	 * Enqueue to be called back when pressure is low enough to
	 * spawn a child process.
	 *
	 * The main implementation of the SpawnChildProcess() method
	 * (i.e. in class #SpawnServerClient) is implemented in a way
	 * that is asynchronous and not cancellable.
	 *
	 * Under heavy pressure, it may take the dedicated spawner
	 * process a long time to work through its long queue, and the
	 * socket buffer may run full.  Meanwhile, many of the reasons
	 * to spawn the child process may be cancelled, but the
	 * cancellation cannot be propagated to the spawner process,
	 * causing it to spawn many processes that will be killed
	 * immediately, increasing pressure further.
	 *
	 * It is preferable to have the queue of callers waiting for
	 * the new child process to be spawned inside the main process
	 * instead of submitting everything to the dedicated spawner
	 * process right away.  This client-side queue can then be
	 * cancelled easy.  The Enqueue() method allows just that.
	 *
	 * This feature opt-in; callers can invoke SpawnChildProcess()
	 * right away, or optionally wait for Enqueue() to call them
	 * back.  Under pressure, this can imply that callers using
	 * Enqueue() will starve.
	 *
	 * A naive implementation will invoke the callback immediately
	 * (inside this method).  Callers must be prepared for that
	 * case.
	 */
	virtual void Enqueue(EnqueueCallback callback, CancellablePointer &cancel_ptr) noexcept = 0;
};
