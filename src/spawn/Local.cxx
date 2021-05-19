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
#include "PidfdEvent.hxx"
#include "ExitListener.hxx"
#include "Config.hxx"
#include "Direct.hxx"
#include "Registry.hxx"
#include "Prepared.hxx"
#include "CgroupState.hxx"
#include "event/PipeEvent.hxx"
#include "io/Logger.hxx"

#include <memory>
#include <utility>

#include <linux/wait.h>
#include <signal.h>
#include <sys/wait.h>

class LocalChildProcess final : public ChildProcessHandle, ExitListener {
	ChildProcessRegistry &registry;

	std::unique_ptr<PidfdEvent> pidfd;

	ExitListener *exit_listener = nullptr;

public:
	LocalChildProcess(EventLoop &event_loop,
			  ChildProcessRegistry &_registry,
			  UniqueFileDescriptor &&_pidfd,
			  const char *_name) noexcept
		:registry(_registry),
		 pidfd(std::make_unique<PidfdEvent>(event_loop,
						    std::move(_pidfd),
						    _name,
						    (ExitListener &)*this))
	{
	}

	~LocalChildProcess() noexcept override {
		if (pidfd)
			Kill(SIGTERM);
	}

private:
	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;

	/* virtual methods from class ChildProcessHandle */
	void SetExitListener(ExitListener &listener) noexcept override {
		assert(pidfd);

		exit_listener = &listener;
	}

	void Kill(int signo) noexcept override;
};

void
LocalChildProcess::OnChildProcessExit(int status) noexcept
{
	pidfd.reset();

	if (exit_listener != nullptr)
		exit_listener->OnChildProcessExit(status);
}

void
LocalChildProcess::Kill(int signo) noexcept
{
	assert(pidfd);

	registry.Kill(std::move(pidfd), signo);
}

std::unique_ptr<ChildProcessHandle>
LocalSpawnService::SpawnChildProcess(const char *name,
				     PreparedChildProcess &&params)
{
	if (params.uid_gid.IsEmpty())
		params.uid_gid = config.default_uid_gid;

	auto pidfd = ::SpawnChildProcess(std::move(params), CgroupState(),
					 false /* TODO? */);
	return std::make_unique<LocalChildProcess>(event_loop, registry,
						   std::move(pidfd),
						   name);
}
