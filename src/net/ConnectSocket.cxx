/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "AddressInfo.hxx"
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
ConnectSocket::Connect(const SocketAddress address)
{
    assert(!fd.IsDefined());

    try {
        WaitConnected(::Connect(address), connect_timeout);
        return true;
    } catch (...) {
        handler.OnSocketConnectError(std::current_exception());
        return false;
    }
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
ConnectSocket::WaitConnected(UniqueSocketDescriptor &&_fd,
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
