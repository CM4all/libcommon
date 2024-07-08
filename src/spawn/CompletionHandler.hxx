// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>

/**
 * This interface gets notified when spawning a child process
 * completes.  Pass a reference to an instance to
 * ChildProcessHandle::SetCompletionHandler().
 */
class SpawnCompletionHandler {
public:
	virtual void OnSpawnSuccess() noexcept = 0;
	virtual void OnSpawnError(std::exception_ptr error) noexcept = 0;
};
