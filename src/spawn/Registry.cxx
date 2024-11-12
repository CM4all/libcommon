// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Registry.hxx"
#include "ExitListener.hxx"
#include "PidfdEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "io/Logger.hxx"

#include <cassert>

#include <signal.h>

static constexpr Event::Duration child_kill_timeout = std::chrono::minutes(1);

class ChildProcessRegistry::KilledChildProcess final
	: public AutoUnlinkIntrusiveListHook, ExitListener
{
	std::unique_ptr<PidfdEvent> pidfd;

	/**
	 * This timer is set up by child_kill_signal().  If the child
	 * process hasn't exited after a certain amount of time, we send
	 * SIGKILL.
	 */
	CoarseTimerEvent kill_timeout_event;

public:
	explicit KilledChildProcess(std::unique_ptr<PidfdEvent> &&_pidfd)
		:pidfd(std::move(_pidfd)),
		 kill_timeout_event(pidfd->GetEventLoop(),
				    BIND_THIS_METHOD(KillTimeoutCallback))
	{
		assert(pidfd);
		pidfd->SetListener(*this);
		kill_timeout_event.Schedule(child_kill_timeout);
	}

	void KillNow() noexcept {
		pidfd->Kill(SIGKILL);
	}

private:
	void KillTimeoutCallback() noexcept;

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int) noexcept override {
		delete this;
	}
};

inline void
ChildProcessRegistry::KilledChildProcess::KillTimeoutCallback() noexcept
{
	pidfd->GetLogger()(3, "sending SIGKILL to due to timeout");
	KillNow();
	delete this;
}

ChildProcessRegistry::ChildProcessRegistry() noexcept = default;

ChildProcessRegistry::~ChildProcessRegistry() noexcept
{
	killed_list.clear_and_dispose([](auto *k){
		k->KillNow();
		delete k;
	});
}

void
ChildProcessRegistry::Kill(std::unique_ptr<PidfdEvent> pidfd,
			   int signo) noexcept
{
	if (!pidfd->Kill(signo))
		return;

	auto *k = new KilledChildProcess(std::move(pidfd));
	killed_list.push_back(*k);
}
