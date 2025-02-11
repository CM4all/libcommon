// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ServerSocket.hxx"
#include "net/IPv4Address.hxx"
#include "net/IPv6Address.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/SocketConfig.hxx"
#include "net/SocketError.hxx"

#include <cassert>

ServerSocket::~ServerSocket() noexcept
{
	event.Close();
}

void
ServerSocket::Listen(UniqueSocketDescriptor _fd) noexcept
{
	assert(!event.IsDefined());
	assert(_fd.IsDefined());

	event.Open(_fd.Release());
	event.ScheduleRead();
}

static UniqueSocketDescriptor
MakeListener(const SocketAddress address,
	     bool reuse_port,
	     bool free_bind,
	     const char *bind_to_device)
{
	constexpr int socktype = SOCK_STREAM;

	SocketConfig config{
		.bind_address = AllocatedSocketAddress{address},
		.listen = 256,
		.reuse_port = reuse_port,
		.free_bind = free_bind,
		.pass_cred = true,
	};

	if (bind_to_device != nullptr)
		config.interface = bind_to_device;

	return config.Create(socktype);
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
	Listen(LocalSocketAddress{path}, false, false, nullptr);
}

void
ServerSocket::EventCallback(unsigned) noexcept
{
	StaticSocketAddress remote_address;
	UniqueSocketDescriptor remote_fd{AdoptTag{}, GetSocket().AcceptNonBlock(remote_address)};
	if (!remote_fd.IsDefined()) {
		const auto e = GetSocketError();
		if (!IsSocketErrorAcceptWouldBlock(e))
			OnAcceptError(std::make_exception_ptr(MakeSocketError(e, "Failed to accept connection")));

		return;
	}

	OnAccept(std::move(remote_fd), remote_address);
}
