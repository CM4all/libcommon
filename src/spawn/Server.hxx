// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct SpawnConfig;
struct CgroupState;
class SpawnHook;
class UniqueSocketDescriptor;

/**
 * Throws on initialization error.
 */
void
RunSpawnServer(const SpawnConfig &config, const CgroupState &cgroup_state,
	       bool has_mount_namespace,
	       SpawnHook *hook,
	       UniqueSocketDescriptor fd);
