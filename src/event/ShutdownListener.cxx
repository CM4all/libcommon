// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ShutdownListener.hxx"

#include <signal.h>
#include <stdio.h>

inline void
ShutdownListener::SignalCallback(int signo) noexcept
{
	fprintf(stderr, "caught signal %d, shutting down (pid=%d)\n",
		signo, (int)getpid());

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
