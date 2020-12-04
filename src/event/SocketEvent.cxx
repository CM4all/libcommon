/*
 * Copyright 2017-2020 CM4all GmbH
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

#include "SocketEvent.hxx"
#include "Loop.hxx"
#include "event/Features.h"

#include <cassert>
#include <utility>

#ifdef USE_EPOLL
#include <cerrno>
#endif

void
SocketEvent::Open(SocketDescriptor _fd) noexcept
{
	assert(_fd.IsDefined());
	assert(!fd.IsDefined());
	assert(GetScheduledFlags() == 0);

	fd = _fd;
}

void
SocketEvent::Close() noexcept
{
	if (!fd.IsDefined())
		return;

	/* closing the socket automatically unregisters it from epoll,
	   so we can omit the epoll_ctl(EPOLL_CTL_DEL) call and save
	   one system call */
	if (std::exchange(scheduled_flags, 0) != 0)
		loop.AbandonFD(*this);

	fd.Close();
}

void
SocketEvent::Abandon() noexcept
{
	if (std::exchange(scheduled_flags, 0) != 0)
		loop.AbandonFD(*this);

	fd = SocketDescriptor::Undefined();
}

bool
SocketEvent::Schedule(unsigned flags) noexcept
{
	if (flags != 0)
		flags |= IMPLICIT_FLAGS;

	if (flags == GetScheduledFlags())
		return true;

	assert(IsDefined());

	bool success;
	if (scheduled_flags == 0)
		success = loop.AddFD(fd.Get(), flags, *this);
	else if (flags == 0)
		success = loop.RemoveFD(fd.Get(), *this);
	else
		success = loop.ModifyFD(fd.Get(), flags, *this);

	if (success)
		scheduled_flags = flags;
#ifdef USE_EPOLL
	else if (errno == EBADF || errno == ENOENT)
		/* the socket was probably closed by somebody else
		   (EBADF) or a new file descriptor with the same
		   number was created but not registered already
		   (ENOENT) - we can assume that there are no
		   scheduled events */
		/* note that when this happens, we're actually lucky
		   that it has failed - imagine another thread may
		   meanwhile have created something on the same file
		   descriptor number, and has registered it; the
		   epoll_ctl() call above would then have succeeded,
		   but broke the other thread's epoll registration */
		scheduled_flags = 0;
#endif

	return success;
}

void
SocketEvent::Dispatch() noexcept
{
	const unsigned flags = std::exchange(ready_flags, 0) &
		GetScheduledFlags();

	if (flags != 0)
		callback(flags);
}
