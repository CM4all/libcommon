// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct PreparedChildProcess;

/**
 * Hooks to be called by the spawn server.
 */
class SpawnHook {
public:
	/**
	 * Verify the #PreparedChildProcess object, especially the #UidGid
	 * settings, whether these credentials are allowed.
	 *
	 * Throws std::runtime_error if the #PreparedChildProcess object
	 * is rejected.
	 *
	 * @return true if the #PreparedChildProcess object is accepted,
	 * false to apply the default checks instead (which may then
	 * result in acceptance or rejection)
	 */
	virtual bool Verify(const PreparedChildProcess &) {
		return false;
	}
};
