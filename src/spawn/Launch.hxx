// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <functional>

struct SpawnConfig;
class SpawnHook;
class UniqueSocketDescriptor;
class UniqueFileDescriptor;

/**
 * Throws on error.
 *
 * @return a pidfd
 */
UniqueFileDescriptor
LaunchSpawnServer(const SpawnConfig &config, SpawnHook *hook,
		  UniqueSocketDescriptor socket,
		  std::function<void()> post_clone);

/**
 * An overload which creates the socketpair automatically and returns
 * the connection (but discards the pidfd).
 *
 * Throws on error.
 *
 * @return a seqpacket socket connection to the spawner
 */
UniqueSocketDescriptor
LaunchSpawnServer(const SpawnConfig &config, SpawnHook *hook);
