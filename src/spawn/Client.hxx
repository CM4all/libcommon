// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Interface.hxx"
#include "Config.hxx"
#include "event/SocketEvent.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/MultiReceiveMessage.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "config.h"

#include <forward_list>
#include <map>
#include <memory>
#include <span>

struct PreparedChildProcess;
class CgroupMemoryWatch;
class SpawnPayload;
class SpawnSerializer;
class SpawnServerClientHandler;

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

	unsigned last_pid = 0;

	using ChildProcessSet =
		IntrusiveHashSet<ChildProcess, 1024,
				 IntrusiveHashSetOperators<ChildProcess,
							   ChildProcessGetKey,
							   std::hash<unsigned>,
							   std::equal_to<unsigned>>>;

	ChildProcessSet processes;

	/**
	 * Filled by KillChildProcess(), consumed by OnSocketEvent()
	 * and FlushKillQueue().
	 */
	std::forward_list<KillQueueItem> kill_queue;

	SocketEvent event;

	MultiReceiveMessage receive{16, 1024, CMSG_SPACE(sizeof(int)), 1};

	SpawnServerClientHandler *handler = nullptr;

#ifdef HAVE_LIBSYSTEMD
	std::unique_ptr<CgroupMemoryWatch> cgroup_memory_watch;
#endif

	/**
	 * Call UidGid::Verify() before sending the spawn request to the
	 * server?
	 */
	const bool verify;

	bool cgroups = false;

	bool shutting_down = false;

public:
	explicit SpawnServerClient(EventLoop &event_loop,
				   const SpawnConfig &_config,
				   UniqueSocketDescriptor _socket,
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

	void SetHandler(SpawnServerClientHandler &_handler) noexcept {
		handler = &_handler;
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

	void OnSocketEvent(unsigned events) noexcept;

	void Kill(ChildProcess &child_process, int signo) noexcept;

	void OnCgroupMemoryWarning(uint64_t memory_usage) noexcept;

public:
	/* virtual methods from class SpawnService */
	std::unique_ptr<ChildProcessHandle> SpawnChildProcess(const char *name, PreparedChildProcess &&params) override;
};
