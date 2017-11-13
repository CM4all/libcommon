/*
 * Copyright 2007-2017 Content Management AG
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

#include "UdpListener.hxx"
#include "UdpHandler.hxx"
#include "net/SocketAddress.hxx"
#include "net/ReceiveMessage.hxx"
#include "system/Error.hxx"

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

UdpListener::UdpListener(EventLoop &event_loop, UniqueSocketDescriptor _fd,
			 UdpHandler &_handler) noexcept
	:fd(std::move(_fd)),
	 event(event_loop, fd.Get(), SocketEvent::READ|SocketEvent::PERSIST,
	       BIND_THIS_METHOD(EventCallback)),
	 handler(_handler)
{
	event.Add();
}

UdpListener::~UdpListener() noexcept
{
	assert(fd.IsDefined());

	event.Delete();
}

bool
UdpListener::ReceiveAll()
try {
	while (true) {
		try {
			if (!ReceiveOne())
				return false;
		} catch (const std::system_error &e) {
			if (e.code().category() == ErrnoCategory() &&
			    e.code().value() == EAGAIN)
				/* no more pending datagrams */
				return true;

			throw;
		}
	}
} catch (...) {
	event.Delete();
	throw;
}

bool
UdpListener::ReceiveOne()
{
	ReceiveMessageBuffer<4096, 1024> buffer;
	auto result = ReceiveMessage(fd, buffer,
				     MSG_DONTWAIT|MSG_CMSG_CLOEXEC);
	result.fds.clear();
	int uid = result.cred != nullptr
		? result.cred->uid
		: -1;
	return handler.OnUdpDatagram(result.payload.data,
				     result.payload.size,
				     result.address,
				     uid);
}

void
UdpListener::EventCallback(unsigned) noexcept
try {
	ReceiveOne();
} catch (...) {
	/* unregister the SocketEvent, just in case the handler does
	   not destroy us */
	event.Delete();

	handler.OnUdpError(std::current_exception());
}

void
UdpListener::Reply(SocketAddress address,
		   const void *data, size_t data_length)
{
	assert(fd.IsDefined());

	ssize_t nbytes = sendto(fd.Get(), data, data_length,
				MSG_DONTWAIT|MSG_NOSIGNAL,
				address.GetAddress(), address.GetSize());
	if (gcc_unlikely(nbytes < 0))
		throw MakeErrno("Failed to send UDP packet");

	if ((size_t)nbytes != data_length)
		throw std::runtime_error("Short send");
}
