/*
 * Copyright 2007-2018 Content Management AG
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

#include "SocketWrapper.hxx"
#include "io/Splice.hxx"
#include "net/Buffered.hxx"

#include <unistd.h>
#include <sys/socket.h>

void
SocketWrapper::SocketEventCallback(unsigned events) noexcept
{
	assert(IsValid());

	if (events & SocketEvent::ERROR) {
		if (!handler.OnSocketError(GetSocket().GetError()))
			return;
	}

	if (events & SocketEvent::READ)
		read_timeout_event.Cancel();

	if (events & SocketEvent::WRITE)
		write_timeout_event.Cancel();

	if (events & SocketEvent::READ) {
		if (!handler.OnSocketRead())
			return;
	}

	if (events & SocketEvent::WRITE) {
		handler.OnSocketWrite();
	}
}

void
SocketWrapper::TimeoutCallback() noexcept
{
	assert(IsValid());

	handler.OnSocketTimeout();
}

void
SocketWrapper::Init(SocketDescriptor fd, FdType _fd_type) noexcept
{
	assert(fd.IsDefined());

	fd_type = _fd_type;

	socket_event.Open(fd);
}

void
SocketWrapper::Shutdown() noexcept
{
	if (!IsValid())
		return;

	shutdown(GetSocket().Get(), SHUT_RDWR);
}

void
SocketWrapper::Close() noexcept
{
	if (!IsValid())
		return;

	socket_event.Cancel();
	read_timeout_event.Cancel();
	write_timeout_event.Cancel();

	socket_event.ReleaseSocket().Close();
}

void
SocketWrapper::Abandon() noexcept
{
	assert(IsValid());

	socket_event.Cancel();
	read_timeout_event.Cancel();
	write_timeout_event.Cancel();

	socket_event.ReleaseSocket();
}

int
SocketWrapper::AsFD() noexcept
{
	assert(IsValid());

	const int result = GetSocket().Get();
	Abandon();
	return result;
}

ssize_t
SocketWrapper::ReadToBuffer(ForeignFifoBuffer<uint8_t> &buffer) noexcept
{
	assert(IsValid());

	return ReceiveToBuffer(GetSocket().Get(), buffer);
}

bool
SocketWrapper::IsReadyForWriting() const noexcept
{
	assert(IsValid());

	return GetSocket().IsReadyForWriting();
}

ssize_t
SocketWrapper::Write(const void *data, size_t length) noexcept
{
	assert(IsValid());

	return send(GetSocket().Get(), data, length, MSG_DONTWAIT|MSG_NOSIGNAL);
}

ssize_t
SocketWrapper::WriteV(const struct iovec *v, size_t n) noexcept
{
	assert(IsValid());

	struct msghdr m = {
		.msg_name = nullptr,
		.msg_namelen = 0,
		.msg_iov = const_cast<struct iovec *>(v),
		.msg_iovlen = n,
		.msg_control = nullptr,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	return sendmsg(GetSocket().Get(), &m, MSG_DONTWAIT|MSG_NOSIGNAL);
}

ssize_t
SocketWrapper::WriteFrom(int other_fd, FdType other_fd_type,
			 size_t length) noexcept
{
	return SpliceToSocket(other_fd_type, other_fd,
			      GetSocket().Get(), length);
}
