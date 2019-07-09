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

#include "ServerSocket.hxx"
#include "net/IPv4Address.hxx"
#include "net/IPv6Address.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/SocketConfig.hxx"
#include "system/Error.hxx"

#include <assert.h>
#include <sys/socket.h>
#include <errno.h>

ServerSocket::~ServerSocket() noexcept = default;

void
ServerSocket::Listen(UniqueSocketDescriptor _fd) noexcept
{
	assert(!fd.IsDefined());
	assert(_fd.IsDefined());

	fd = std::move(_fd);
	event.Open(fd);
	AddEvent();
}

static UniqueSocketDescriptor
MakeListener(const SocketAddress address,
	     bool reuse_port,
	     bool free_bind,
	     const char *bind_to_device)
{
	constexpr int socktype = SOCK_STREAM;

	SocketConfig config(address);

	if (bind_to_device != nullptr)
		config.interface = bind_to_device;

	config.reuse_port = reuse_port;
	config.free_bind = free_bind;
	config.pass_cred = true;
	config.listen = 64;

	return config.Create(socktype);
}

static bool
IsTCP(SocketAddress address) noexcept
{
	return address.GetFamily() == AF_INET || address.GetFamily() == AF_INET6;
}

void
ServerSocket::Listen(SocketAddress address,
		     bool reuse_port,
		     bool free_bind,
		     const char *bind_to_device)
{
	Listen(MakeListener(address, reuse_port, free_bind, bind_to_device));
}

void
ServerSocket::ListenTCP(unsigned port)
{
	try {
		ListenTCP6(port);
	} catch (...) {
		ListenTCP4(port);
	}
}

void
ServerSocket::ListenTCP4(unsigned port)
{
	assert(port > 0);

	Listen(IPv4Address(port),
	       false, false, nullptr);
}

void
ServerSocket::ListenTCP6(unsigned port)
{
	assert(port > 0);

	Listen(IPv6Address(port),
	       false, false, nullptr);
}

void
ServerSocket::ListenPath(const char *path)
{
	AllocatedSocketAddress address;
	address.SetLocal(path);

	Listen(address, false, false, nullptr);
}

StaticSocketAddress
ServerSocket::GetLocalAddress() const noexcept
{
	return fd.GetLocalAddress();
}

void
ServerSocket::EventCallback(unsigned) noexcept
{
	StaticSocketAddress remote_address;
	auto remote_fd = fd.AcceptNonBlock(remote_address);
	if (!remote_fd.IsDefined()) {
		const int e = errno;
		if (e != EAGAIN && e != EWOULDBLOCK)
			OnAcceptError(std::make_exception_ptr(MakeErrno(e, "Failed to accept connection")));

		return;
	}

	if (IsTCP(remote_address) && !remote_fd.SetNoDelay()) {
		OnAcceptError(std::make_exception_ptr(MakeErrno("setsockopt(TCP_NODELAY) failed")));
		return;
	}

	OnAccept(std::move(remote_fd), remote_address);
}
