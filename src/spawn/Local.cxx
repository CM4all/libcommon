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
#include "Terminator.hxx"
#include "Prepared.hxx"
#include "CgroupState.hxx"
#include "event/PipeEvent.hxx"
#include "io/FileAt.hxx"
#include "io/Logger.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/WriteFile.hxx"
#include "co/InvokeTask.hxx"
#include "co/Task.hxx"

#include <memory>
#include <string>
#include <utility>

#include <signal.h>
#include <sys/wait.h>

using std::string_view_literals::operator""sv;

class LocalChildProcess final : public ChildProcessHandle, ExitListener {
	EventLoop &event_loop;
	ChildProcessTerminator &terminator;

	const std::string name;

	Co::InvokeTask invoke_task;
	std::exception_ptr error;

	std::unique_ptr<PidfdEvent> pidfd;

	UniqueFileDescriptor session_cgroup_fd;
	UniqueFileDescriptor accessory_lease_pipe;

	SpawnCompletionHandler *completion_handler = nullptr;
	ExitListener *exit_listener = nullptr;

	const bool sigkill;

public:
	LocalChildProcess(EventLoop &_event_loop,
			  ChildProcessTerminator &_terminator,
			  std::string_view _name,
			  bool _sigkill) noexcept
		:event_loop(_event_loop), terminator(_terminator),
		 name(_name),
		 sigkill(_sigkill)
	{
	}

	~LocalChildProcess() noexcept override {
		Kill(SIGTERM);
	}

	void Start(Co::Task<SpawnChildProcessResult> &&task) {
		invoke_task = Await(std::move(task));
		invoke_task.Start(BIND_THIS_METHOD(OnCompletion));
	}

private:
	Co::InvokeTask Await(Co::Task<SpawnChildProcessResult> task) {
		auto result = co_await task;

		ExitListener &_exit_listener = *this;
		pidfd = std::make_unique<PidfdEvent>(event_loop,
						     std::move(result.pidfd),
						     name,
						     _exit_listener);

		if (sigkill)
			session_cgroup_fd = std::move(result.session_cgroup_fd);

		accessory_lease_pipe = std::move(result.accessory_lease_pipe);
	}

	void OnCompletion(std::exception_ptr &&_error) noexcept {
		if (completion_handler != nullptr) {
			if (_error)
				completion_handler->OnSpawnError(std::move(_error));
			else
				completion_handler->OnSpawnSuccess();
		} else
			error = std::move(_error);
	}

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;

	/* virtual methods from class ChildProcessHandle */
	void SetCompletionHandler(SpawnCompletionHandler &_handler) noexcept override {
		if (invoke_task)
			completion_handler = &_handler;
		else if (error)
			_handler.OnSpawnError(std::move(error));
		else
			_handler.OnSpawnSuccess();
	}

	void SetExitListener(ExitListener &listener) noexcept override {
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
	if (sigkill && session_cgroup_fd.IsDefined() &&
	    TryWriteExistingFile({session_cgroup_fd, "cgroup.kill"},
				 "1"sv) == WriteFileResult::SUCCESS)
		return;

	if (pidfd)
		terminator.Kill(std::move(pidfd), signo);
}

std::unique_ptr<ChildProcessHandle>
LocalSpawnService::SpawnChildProcess(std::string_view name,
				     PreparedChildProcess &&params)
{
	if (params.uid_gid.IsEmpty())
		params.uid_gid = config.default_uid_gid;

	auto task = ::SpawnChildProcess(event_loop,
					std::move(params), CgroupState(),
					false,
					false /* TODO? */);

	auto handle = std::make_unique<LocalChildProcess>(event_loop, terminator,
							  name, params.sigkill);
	handle->Start(std::move(task));
	return handle;
}

void
LocalSpawnService::Enqueue(EnqueueCallback callback, CancellablePointer &) noexcept
{
	callback();
}
