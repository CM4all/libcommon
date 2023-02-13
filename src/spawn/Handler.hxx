// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

/**
 * Handler for #SpawnServerClient.
 */
class SpawnServerClientHandler {
public:
	virtual void OnMemoryWarning(uint64_t memory_usage,
				     uint64_t memory_max) noexcept = 0;
};
