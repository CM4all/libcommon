/*
 * Copyright 2007-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BENG_PROXY_SPAWN_CLIENT_HXX
#define BENG_PROXY_SPAWN_CLIENT_HXX

#include "Interface.hxx"
#include "Config.hxx"
#include "event/SocketEvent.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <map>

template<typename T> struct ConstBuffer;
struct PreparedChildProcess;
class SpawnPayload;
class SpawnSerializer;

class SpawnServerClient final : public SpawnService {
	struct ChildProcess {
		ExitListener *listener;

		explicit ChildProcess(ExitListener *_listener)
			:listener(_listener) {}
	};

	const SpawnConfig config;

	UniqueSocketDescriptor socket;

	unsigned last_pid = 0;

	std::map<int, ChildProcess> processes;

	SocketEvent event;

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
				   bool _verify=true);
	~SpawnServerClient();

	/**
	 * Does the server support cgroups?  This requires a systemd new
	 * enough to implement the cgroup management protocol.
	 *
	 * Note that this information is only available after receiving
	 * the SpawnResponseCommand::CGROUPS_AVAILABLE packet.  Don't rely
	 * on its value right after creating the spawner.
	 */
	bool SupportsCgroups() const {
		return cgroups;
	}

	void ReplaceSocket(UniqueSocketDescriptor new_socket);

	void Shutdown();

	UniqueSocketDescriptor Connect();

private:
	int MakePid() {
		++last_pid;
		if (last_pid >= 0x40000000)
			last_pid = 1;
		return last_pid;
	}

	void Close();

	/**
	 * Check if the spawner is alive, and if not, commit suicide, and
	 * hope this daemon gets restarted automatically with a fresh
	 * spawner; there's not much else we can do without a spawner.
	 * Failing hard and awaiting a restart is better than failing
	 * softly over and over.
	 */
	void CheckOrAbort();

	void Send(ConstBuffer<void> payload, ConstBuffer<int> fds);
	void Send(const SpawnSerializer &s);

	void HandleExitMessage(SpawnPayload payload);
	void HandleMessage(ConstBuffer<uint8_t> payload);
	void OnSocketEvent(unsigned events);

public:
	/* virtual methods from class SpawnService */
	int SpawnChildProcess(const char *name, PreparedChildProcess &&params,
			      ExitListener *listener) override;

	void SetExitListener(int pid, ExitListener *listener) override;

	void KillChildProcess(int pid, int signo) override;
};

#endif
