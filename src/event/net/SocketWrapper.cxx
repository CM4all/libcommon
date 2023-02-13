// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SocketWrapper.hxx"
#include "io/Splice.hxx"
#include "net/Buffered.hxx"
#include "net/MsgHdr.hxx"
#include "net/UniqueSocketDescriptor.hxx"

void
SocketWrapper::SocketEventCallback(unsigned events) noexcept
{
	assert(IsValid());

	if (events & SocketEvent::ERROR) {
		if (!handler.OnSocketError(GetSocket().GetError()))
			return;

		/* update the "events" bitmask, just in case
		   OnSocketError() has unscheduled events which are
		   still in this local variable */
		events &= socket_event.GetScheduledFlags();
	}

	if (events & SocketEvent::HANGUP) {
		if (!handler.OnSocketHangup())
			return;

		/* update the "events" bitmask, just in case
		   OnSocketHangup() has unscheduled events which are
		   still in this local variable */
		events &= socket_event.GetScheduledFlags();
	}

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
SocketWrapper::Init(UniqueSocketDescriptor _fd, FdType _fd_type) noexcept
{
	Init(_fd.Release(), _fd_type);
}

void
SocketWrapper::Shutdown() noexcept
{
	if (!IsValid())
		return;

	GetSocket().Shutdown();
}

void
SocketWrapper::Close() noexcept
{
	if (!IsValid())
		return;

	socket_event.Close();
	write_timeout_event.Cancel();
}

void
SocketWrapper::Abandon() noexcept
{
	assert(IsValid());

	socket_event.Cancel();
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
SocketWrapper::ReadToBuffer(ForeignFifoBuffer<std::byte> &buffer) noexcept
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
SocketWrapper::Write(const void *data, std::size_t length) noexcept
{
	assert(IsValid());

	return send(GetSocket().Get(), data, length, MSG_DONTWAIT|MSG_NOSIGNAL);
}

ssize_t
SocketWrapper::WriteV(const struct iovec *v, std::size_t n) noexcept
{
	assert(IsValid());

	auto m = MakeMsgHdr({v, n});

	return sendmsg(GetSocket().Get(), &m, MSG_DONTWAIT|MSG_NOSIGNAL);
}

ssize_t
SocketWrapper::WriteFrom(FileDescriptor other_fd, FdType other_fd_type,
			 off_t *other_offset,
			 std::size_t length) noexcept
{
	return SpliceToSocket(other_fd_type, other_fd, other_offset,
			      GetSocket().ToFileDescriptor(),
			      length);
}
