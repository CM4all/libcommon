// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ShutdownListener.hxx"

#include <fmt/core.h>

#include <signal.h>
#include <stdio.h>

inline void
ShutdownListener::SignalCallback(int signo) noexcept
{
	fmt::print(stderr, "caught signal {}, shutting down (pid={})\n",
		   signo, getpid());

	Disable();
	callback();
}

ShutdownListener::ShutdownListener(EventLoop &loop,
				   Callback _callback) noexcept
	:event(loop, BIND_THIS_METHOD(SignalCallback)),
	 callback(_callback)
{
	event.Add(SIGTERM);
	event.Add(SIGINT);
	event.Add(SIGQUIT);
}
