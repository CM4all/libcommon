// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Terminator.hxx"
#include "ExitListener.hxx"
#include "PidfdEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/Loop.hxx"
#include "io/Logger.hxx"

#include <cassert>

#include <signal.h>

static constexpr Event::Duration child_kill_timeout = std::chrono::minutes(1);

class ChildProcessTerminator::KilledChildProcess final
	: public AutoUnlinkIntrusiveListHook, ExitListener
{
	ChildProcessTerminator &parent;

	std::unique_ptr<PidfdEvent> pidfd;

	/**
	 * This timer is set up by child_kill_signal().  If the child
	 * process hasn't exited after a certain amount of time, we send
	 * SIGKILL.
	 */
	CoarseTimerEvent kill_timeout_event;

	const Event::TimePoint start_time;

public:
	explicit KilledChildProcess(ChildProcessTerminator &_parent,
				    std::unique_ptr<PidfdEvent> &&_pidfd)
		:parent(_parent),
		 pidfd(std::move(_pidfd)),
		 kill_timeout_event(pidfd->GetEventLoop(),
				    BIND_THIS_METHOD(KillTimeoutCallback)),
		 start_time(kill_timeout_event.GetEventLoop().SteadyNow())
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
		++parent.stats.n_exits;
		++parent.stats.total_shutdown_duration += kill_timeout_event.GetEventLoop().SteadyNow() - start_time;
		delete this;
	}
};

inline void
ChildProcessTerminator::KilledChildProcess::KillTimeoutCallback() noexcept
{
	++parent.stats.n_timeouts;
	++parent.stats.total_shutdown_duration += kill_timeout_event.GetEventLoop().SteadyNow() - start_time;

	pidfd->GetLogger()(3, "sending SIGKILL to due to timeout");
	KillNow();
	delete this;
}

ChildProcessTerminator::ChildProcessTerminator() noexcept = default;

ChildProcessTerminator::~ChildProcessTerminator() noexcept
{
	killed_list.clear_and_dispose([](auto *k){
		k->KillNow();
		delete k;
	});
}

void
ChildProcessTerminator::Kill(std::unique_ptr<PidfdEvent> pidfd,
			   int signo) noexcept
{
	++stats.n_signals;

	if (!pidfd->Kill(signo)) {
		++stats.n_failed_signals;
		return;
	}

	auto *k = new KilledChildProcess(*this, std::move(pidfd));
	killed_list.push_back(*k);
}
