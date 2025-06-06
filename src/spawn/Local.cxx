// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Local.hxx"
#include "ProcessHandle.hxx"
#include "CompletionHandler.hxx"
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
			  std::string_view _name) noexcept
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
	void SetCompletionHandler(SpawnCompletionHandler &handler) noexcept override {
		assert(pidfd);

		handler.OnSpawnSuccess();
	}

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
LocalSpawnService::SpawnChildProcess(std::string_view name,
				     PreparedChildProcess &&params)
{
	if (params.uid_gid.IsEmpty())
		params.uid_gid = config.default_uid_gid;

	auto pidfd = ::SpawnChildProcess(std::move(params), CgroupState(),
					 false,
					 false /* TODO? */).first;
	return std::make_unique<LocalChildProcess>(event_loop, registry,
						   std::move(pidfd),
						   name);
}

void
LocalSpawnService::Enqueue(EnqueueCallback callback, CancellablePointer &) noexcept
{
	callback();
}
