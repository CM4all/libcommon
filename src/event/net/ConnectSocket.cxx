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

#include "ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "net/AddressInfo.hxx"
#include "system/Error.hxx"

#include <stdexcept>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static constexpr timeval connect_timeout{10, 0};

void
ConnectSocketHandler::OnSocketConnectTimeout()
{
	/* default implementation falls back to OnSocketConnectError() */
	OnSocketConnectError(std::make_exception_ptr(std::runtime_error("Connect timeout")));
}

ConnectSocket::ConnectSocket(EventLoop &_event_loop,
			     ConnectSocketHandler &_handler)
	:handler(_handler),
	 event(_event_loop, BIND_THIS_METHOD(OnEvent))
{
}

ConnectSocket::~ConnectSocket()
{
	if (IsPending())
		Cancel();
}

void
ConnectSocket::Cancel()
{
	assert(IsPending());

	event.Delete();
	fd.Close();
}

static UniqueSocketDescriptor
Connect(const SocketAddress address)
{
	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(address.GetFamily(), SOCK_STREAM, 0))
		throw MakeErrno("Failed to create socket");

	if (!fd.Connect(address) && errno != EINPROGRESS)
		throw MakeErrno("Failed to connect");

	return fd;
}

bool
ConnectSocket::Connect(const SocketAddress address,
		       const struct timeval &timeout)
{
	assert(!fd.IsDefined());

	try {
		WaitConnected(::Connect(address), timeout);
		return true;
	} catch (...) {
		handler.OnSocketConnectError(std::current_exception());
		return false;
	}
}

bool
ConnectSocket::Connect(const SocketAddress address)
{
	return Connect(address, connect_timeout);
}

static UniqueSocketDescriptor
Connect(const AddressInfo &address)
{
	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(address.GetFamily(), address.GetType(),
			       address.GetProtocol()))
		throw MakeErrno("Failed to create socket");

	if (!fd.Connect(address) && errno != EINPROGRESS)
		throw MakeErrno("Failed to connect");

	return fd;
}

bool
ConnectSocket::Connect(const AddressInfo &address,
		       const struct timeval *timeout)
{
	assert(!fd.IsDefined());

	try {
		WaitConnected(::Connect(address), timeout);
		return true;
	} catch (...) {
		handler.OnSocketConnectError(std::current_exception());
		return false;
	}
}

void
ConnectSocket::WaitConnected(UniqueSocketDescriptor _fd,
			     const struct timeval *timeout)
{
	assert(!fd.IsDefined());

	fd = std::move(_fd);
	event.Set(fd.Get(), SocketEvent::WRITE);
	event.Add(timeout);
}

void
ConnectSocket::OnEvent(unsigned events)
{
	if (events & SocketEvent::TIMEOUT) {
		handler.OnSocketConnectTimeout();
		return;
	}

	int s_err = fd.GetError();
	if (s_err != 0) {
		handler.OnSocketConnectError(std::make_exception_ptr(MakeErrno(s_err, "Failed to connect")));
		return;
	}

	handler.OnSocketConnectSuccess(std::exchange(fd,
						     UniqueSocketDescriptor()));
}
