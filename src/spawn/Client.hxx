// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Interface.hxx"
#include "Config.hxx"
#include "Stats.hxx"
#include "event/DeferEvent.hxx"
#include "event/SocketEvent.hxx"
#include "net/MultiReceiveMessage.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/IntrusiveList.hxx"
#include "config.h"

#include <forward_list>
#include <map>
#include <memory>
#include <span>

struct PreparedChildProcess;
class SpawnPayload;
class SpawnSerializer;
class UniqueFileDescriptor;
class UniqueSocketDescriptor;

class SpawnServerClient final : public SpawnService {
	struct ChildProcess;

	struct ChildProcessGetKey {
		[[gnu::pure]]
		unsigned operator()(const ChildProcess &i) const noexcept;
	};

	struct KillQueueItem {
		unsigned pid;
		int signo;
	};

	const SpawnConfig config;

	class SpawnQueueItem;
	IntrusiveList<SpawnQueueItem> spawn_queue;

	using ChildProcessSet =
		IntrusiveHashSet<ChildProcess, 1024,
				 IntrusiveHashSetOperators<ChildProcess,
							   ChildProcessGetKey,
							   std::hash<unsigned>,
							   std::equal_to<unsigned>>,
				 IntrusiveHashSetBaseHookTraits<ChildProcess>,
				 IntrusiveHashSetOptions{.constant_time_size=true}>;

	ChildProcessSet processes;

	/**
	 * Filled by KillChildProcess(), consumed by OnSocketEvent()
	 * and FlushKillQueue().
	 */
	std::forward_list<KillQueueItem> kill_queue;

	SocketEvent event;

	DeferEvent defer_spawn_queue;

	MultiReceiveMessage receive{16, 1024, CMSG_SPACE(sizeof(int)), 1};

	mutable SpawnStats stats{};

	unsigned last_pid = 0;

	/**
	 * The number of EXEC commands that were sent but were not yet
	 * acknowledged.
	 */
	unsigned n_pending_execs = 0;

	static constexpr unsigned THROTTLE_EXECS_THRESHOLD = 8;

	const bool cgroups;

	/**
	 * Call UidGid::Verify() before sending the spawn request to the
	 * server?
	 */
	const bool verify;

	bool shutting_down = false;

public:
	explicit SpawnServerClient(EventLoop &event_loop,
				   const SpawnConfig &_config,
				   UniqueSocketDescriptor _socket,
				   bool _cgroups,
				   bool _verify=true) noexcept;
	~SpawnServerClient() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	/**
	 * Does the server support cgroups?  This requires a systemd new
	 * enough to implement the cgroup management protocol.
	 *
	 * Note that this information is only available after receiving
	 * the SpawnResponseCommand::CGROUPS_AVAILABLE packet.  Don't rely
	 * on its value right after creating the spawner.
	 */
	bool SupportsCgroups() const noexcept {
		return cgroups;
	}

	const SpawnStats &GetStats() const noexcept {
		stats.alive = processes.size();
		return stats;
	}

	void Shutdown() noexcept;

	UniqueSocketDescriptor Connect();

private:
	unsigned MakePid() noexcept {
		++last_pid;
		if (last_pid >= 0x40000000)
			last_pid = 1;
		return last_pid;
	}

	void Close() noexcept;

	/**
	 * If #shutting_down is set and all I/O is complete, close the
	 * connection and return true.
	 */
	bool ShutdownComplete() noexcept;

	/**
	 * Check if the spawner is alive, and if not, commit suicide, and
	 * hope this daemon gets restarted automatically with a fresh
	 * spawner; there's not much else we can do without a spawner.
	 * Failing hard and awaiting a restart is better than failing
	 * softly over and over.
	 */
	void CheckOrAbort() noexcept;

	void Send(std::span<const std::byte> payload,
		  std::span<const FileDescriptor> fds);
	void Send(const SpawnSerializer &s);

	void HandleExecCompleteMessage(SpawnPayload payload);
	void HandleOneExit(SpawnPayload &payload);
	void HandleExitMessage(SpawnPayload payload);
	void HandleMessage(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds);

	/**
	 * Throws on error.
	 */
	void FlushKillQueue();

	/**
	 * Throws on error.
	 */
	void ReceiveAndHandle();

	bool IsUnderPressure() noexcept {
		return n_pending_execs >= THROTTLE_EXECS_THRESHOLD;
	}

	void OnDeferredSpawnQueue() noexcept;

	void OnSocketEvent(unsigned events) noexcept;

	void Kill(ChildProcess &child_process, int signo) noexcept;

public:
	/* virtual methods from class SpawnService */
	std::unique_ptr<ChildProcessHandle> SpawnChildProcess(std::string_view name,
							      PreparedChildProcess &&params) override;
	void Enqueue(EnqueueCallback callback, CancellablePointer &cancel_ptr) noexcept override;
};
