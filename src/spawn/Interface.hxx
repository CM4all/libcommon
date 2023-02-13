// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <memory>

struct PreparedChildProcess;
class ChildProcessHandle;

/**
 * A service which can spawn new child processes according to a
 * #PreparedChildProcess instance.
 */
class SpawnService {
public:
	/**
	 * Throws std::runtime_error on error.
	 *
	 * @return a process id
	 */
	[[nodiscard]]
	virtual std::unique_ptr<ChildProcessHandle> SpawnChildProcess(const char *name,
								      PreparedChildProcess &&params) = 0;
};
