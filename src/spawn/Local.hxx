// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Interface.hxx"

struct SpawnConfig;
class EventLoop;
class ChildProcessTerminator;

class LocalSpawnService final : public SpawnService {
	const SpawnConfig &config;

	EventLoop &event_loop;
	ChildProcessTerminator &terminator;

public:
	explicit LocalSpawnService(const SpawnConfig &_config,
				   EventLoop &_event_loop,
				   ChildProcessTerminator &_terminator) noexcept
		:config(_config),
		 event_loop(_event_loop),
		 terminator(_terminator) {}

	/* virtual methods from class SpawnService */
	std::unique_ptr<ChildProcessHandle> SpawnChildProcess(std::string_view name,
							      PreparedChildProcess &&params) override;
	void Enqueue(EnqueueCallback callback, CancellablePointer &cancel_ptr) noexcept override;
};
