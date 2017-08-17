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

#include "ServerSocket.hxx"
#include "IPv4Address.hxx"
#include "IPv6Address.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "system/Error.hxx"

#include <assert.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

ServerSocket::~ServerSocket()
{
    if (fd.IsDefined())
        event.Delete();
}

void
ServerSocket::Listen(UniqueSocketDescriptor &&_fd)
{
    assert(!fd.IsDefined());
    assert(_fd.IsDefined());

    fd = std::move(_fd);
    event.Set(fd.Get(), SocketEvent::READ|SocketEvent::PERSIST);
    AddEvent();
}

static UniqueSocketDescriptor
MakeListener(const SocketAddress address,
             bool reuse_port,
             bool free_bind,
             const char *bind_to_device)
{
    const int family = address.GetFamily();
    constexpr int socktype = SOCK_STREAM;
    constexpr int protocol = 0;

    if (family == AF_LOCAL) {
        const struct sockaddr_un *sun = (const struct sockaddr_un *)address.GetAddress();
        if (sun->sun_path[0] != '\0')
            /* delete non-abstract socket files before reusing them */
            unlink(sun->sun_path);
    }

    UniqueSocketDescriptor fd;
    if (!fd.CreateNonBlock(family, socktype, protocol))
        throw MakeErrno("Failed to create socket");

    if (!fd.SetReuseAddress(true))
        throw MakeErrno("Failed to set SO_REUSEADDR");

    if (reuse_port && !fd.SetReusePort())
        throw MakeErrno("Failed to set SO_REUSEPORT");

    if (free_bind && !fd.SetFreeBind())
        throw MakeErrno("Failed to set SO_FREEBIND");

    if (address.IsV6Any())
        fd.SetV6Only(false);

    if (bind_to_device != nullptr && !fd.SetBindToDevice(bind_to_device))
        throw MakeErrno("Failed to set SO_BINDTODEVICE");

    if (!fd.Bind(address))
        throw MakeErrno("Failed to bind");

    switch (address.GetFamily()) {
    case AF_INET:
    case AF_INET6:
        if (socktype == SOCK_STREAM)
            fd.SetTcpFastOpen();
        break;

    case AF_LOCAL:
        fd.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);
        break;
    }

    if (!fd.Listen(64))
        throw MakeErrno("Failed to listen");

    return fd;
}

static bool
IsTCP(SocketAddress address)
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
ServerSocket::GetLocalAddress() const
{
    return fd.GetLocalAddress();
}

void
ServerSocket::EventCallback(unsigned)
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
