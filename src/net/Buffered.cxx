// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Buffered.hxx"
#include "SocketDescriptor.hxx"
#include "SocketError.hxx"
#include "util/ForeignFifoBuffer.hxx"

#include <cassert>

ssize_t
ReceiveToBuffer(const SocketDescriptor s,
		ForeignFifoBuffer<std::byte> &buffer) noexcept
{
	assert(s.IsDefined());

	auto w = buffer.Write();
	if (w.empty())
		return -2;

	ssize_t nbytes = s.ReadNoWait(w);
	if (nbytes > 0)
		buffer.Append((size_t)nbytes);

	return nbytes;
}

ssize_t
SendFromBuffer(const SocketDescriptor s,
	       ForeignFifoBuffer<std::byte> &buffer) noexcept
{
	assert(s.IsDefined());

	auto r = buffer.Read();
	if (r.empty())
		return -2;

	ssize_t nbytes = s.WriteNoWait(r);
	if (nbytes >= 0)
		buffer.Consume((size_t)nbytes);
	else if (IsSocketErrorSendWouldBlock(GetSocketError()))
		nbytes = 0;

	return nbytes;
}
