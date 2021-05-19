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

#include "Registry.hxx"
#include "ExitListener.hxx"
#include "PidfdEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "io/Logger.hxx"

#include <cassert>
#include <chrono>

#include <signal.h>

static constexpr auto child_kill_timeout = std::chrono::minutes(1);

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
