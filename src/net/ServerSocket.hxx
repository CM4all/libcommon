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

#ifndef SERVER_SOCKET_HXX
#define SERVER_SOCKET_HXX

#include "net/UniqueSocketDescriptor.hxx"
#include "event/SocketEvent.hxx"

#include <exception>

class SocketAddress;

/**
 * A socket that accepts incoming connections.
 */
class ServerSocket {
    UniqueSocketDescriptor fd;
    SocketEvent event;

public:
    explicit ServerSocket(EventLoop &event_loop)
        :event(event_loop, BIND_THIS_METHOD(EventCallback)) {}

    ~ServerSocket();

    void Listen(UniqueSocketDescriptor &&_fd);

    /**
     * Throws std::runtime_error on error.
     */
    void Listen(SocketAddress address,
                bool reuse_port=false,
                bool free_bind=false,
                const char *bind_to_device=nullptr);

    void ListenTCP(unsigned port);
    void ListenTCP4(unsigned port);
    void ListenTCP6(unsigned port);

    void ListenPath(const char *path);

    StaticSocketAddress GetLocalAddress() const;

    bool SetTcpDeferAccept(const int &seconds) {
        return fd.SetTcpDeferAccept(seconds);
    }

    void AddEvent() {
        event.Add();
    }

    void RemoveEvent() {
        event.Delete();
    }

protected:
    /**
     * A new incoming connection has been established.
     *
     * @param fd the socket owned by the callee
     */
    virtual void OnAccept(UniqueSocketDescriptor &&fd,
                          SocketAddress address) = 0;
    virtual void OnAcceptError(std::exception_ptr ep) = 0;

private:
    void EventCallback(unsigned events);
};

#endif
