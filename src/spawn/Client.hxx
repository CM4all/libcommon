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

#include <forward_list>
#include <map>
#include <span>

struct PreparedChildProcess;
class SpawnPayload;
class SpawnSerializer;
class SpawnServerClientHandler;

class SpawnServerClient final : public SpawnService {
	struct ChildProcess;

	struct ChildProcessHash {
		constexpr std::size_t operator()(unsigned i) const noexcept {
			return i;
		}

		[[gnu::pure]]
		std::size_t operator()(const ChildProcess &i) const noexcept;
	};

	struct ChildProcessEqual{
		[[gnu::pure]]
		bool operator()(const unsigned a,
				const ChildProcess &b) const noexcept;
	};

	struct KillQueueItem {
		unsigned pid;
		int signo;
	};

	const SpawnConfig config;

	unsigned last_pid = 0;

	using ChildProcessSet =
		IntrusiveHashSet<ChildProcess, 1021,
				 ChildProcessHash, ChildProcessEqual>;

	ChildProcessSet processes;

	/**
	 * Filled by KillChildProcess(), consumed by OnSocketEvent()
	 * and FlushKillQueue().
	 */
	std::forward_list<KillQueueItem> kill_queue;

	SocketEvent event;

	MultiReceiveMessage receive{16, 1024};

	SpawnServerClientHandler *handler = nullptr;

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
	void HandleMessage(std::span<const std::byte> payload);

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

public:
	/* virtual methods from class SpawnService */
	std::unique_ptr<ChildProcessHandle> SpawnChildProcess(const char *name, PreparedChildProcess &&params) override;
};
