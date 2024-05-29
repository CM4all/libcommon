// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/UniqueSocketDescriptor.hxx"
#include "io/UniqueFileDescriptor.hxx"

struct SpawnConfig;
class SpawnHook;
class UniqueSocketDescriptor;

struct LaunchSpawnServerResult {
	UniqueFileDescriptor pidfd;

	/**
	 * A seqpacket socket connection to the spawner.
	 */
	UniqueSocketDescriptor socket;

	/**
	 * An O_PATH file descriptor of the cgroup managed by the
	 * spawner.
	 */
	UniqueFileDescriptor cgroup;
};

/**
 * An overload which creates the socketpair automatically and returns
 * the connection (but discards the pidfd).
 *
 * Throws on error.
 */
LaunchSpawnServerResult
LaunchSpawnServer(const SpawnConfig &config, SpawnHook *hook);
