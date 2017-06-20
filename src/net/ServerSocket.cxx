/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "ServerSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "system/Error.hxx"

#include <assert.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
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
                     const char *bind_to_device)
{
    Listen(MakeListener(address, reuse_port, bind_to_device));
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

    struct sockaddr_in sa4;

    memset(&sa4, 0, sizeof(sa4));
    sa4.sin_family = AF_INET;
    sa4.sin_addr.s_addr = INADDR_ANY;
    sa4.sin_port = htons(port);

    Listen(SocketAddress((const struct sockaddr *)&sa4, sizeof(sa4)),
           false, nullptr);
}

void
ServerSocket::ListenTCP6(unsigned port)
{
    assert(port > 0);

    struct sockaddr_in6 sa6;

    memset(&sa6, 0, sizeof(sa6));
    sa6.sin6_family = AF_INET6;
    sa6.sin6_addr = in6addr_any;
    sa6.sin6_port = htons(port);

    Listen(SocketAddress((const struct sockaddr *)&sa6, sizeof(sa6)),
           false, nullptr);
}

void
ServerSocket::ListenPath(const char *path)
{
    AllocatedSocketAddress address;
    address.SetLocal(path);

    Listen(address, false, nullptr);
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
