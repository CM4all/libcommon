/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "Local.hxx"
#include "ProcessHandle.hxx"
#include "ExitListener.hxx"
#include "Config.hxx"
#include "Direct.hxx"
#include "Registry.hxx"
#include "Prepared.hxx"
#include "CgroupState.hxx"

#include <utility>

class LocalChildProcess final : public ChildProcessHandle, ExitListener {
	ChildProcessRegistry &registry;

	pid_t pid;

	ExitListener *exit_listener = nullptr;

public:
	LocalChildProcess(ChildProcessRegistry &_registry,
			  pid_t _pid, const char *name) noexcept
		:registry(_registry), pid(_pid)
	{
		registry.Add(pid, name, this);
	}

	~LocalChildProcess() noexcept override {
		if (pid > 0)
			Kill(SIGTERM);
	}

	/* virtual methods from class ChildProcessHandle */
	void SetExitListener(ExitListener &listener) noexcept override {
		assert(pid > 0);

		exit_listener = &listener;
	}

	void Kill(int signo) noexcept override {
		assert(pid > 0);

		registry.Kill(std::exchange(pid, 0), signo);
	}

	/* virtual methods from class ExitListener */
	void OnChildProcessExit(int status) noexcept override {
		assert(pid > 0);
		pid = 0;

		if (exit_listener != nullptr)
			exit_listener->OnChildProcessExit(status);
	}
};

std::unique_ptr<ChildProcessHandle>
LocalSpawnService::SpawnChildProcess(const char *name,
				     PreparedChildProcess &&params)
{
	if (params.uid_gid.IsEmpty())
		params.uid_gid = config.default_uid_gid;

	pid_t pid = ::SpawnChildProcess(std::move(params), CgroupState(),
					false /* TODO? */);
	return std::make_unique<LocalChildProcess>(registry, pid, name);
}
