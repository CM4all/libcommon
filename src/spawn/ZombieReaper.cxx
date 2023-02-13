// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ZombieReaper.hxx"

#include <signal.h>
#include <sys/wait.h>

ZombieReaper::ZombieReaper(EventLoop &event_loop)
	:sigchld(event_loop, SIGCHLD, BIND_THIS_METHOD(OnSigchld)),
	 defer_wait(event_loop, BIND_THIS_METHOD(DoReap))
{
	sigchld.Enable();
}

void
ZombieReaper::DoReap() noexcept
{
	int wstatus;
	while (waitpid(-1, &wstatus, WNOHANG) > 0) {}
}
