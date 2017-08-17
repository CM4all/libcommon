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

#ifndef SOCKET_EVENT_HXX
#define SOCKET_EVENT_HXX

#include "Event.hxx"
#include "util/BindMethod.hxx"

class SocketEvent {
    EventLoop &event_loop;

    Event event;

    typedef BoundMethod<void(unsigned events)> Callback;
    const Callback callback;

public:
    static constexpr unsigned READ = EV_READ;
    static constexpr unsigned WRITE = EV_WRITE;
    static constexpr unsigned PERSIST = EV_PERSIST;
    static constexpr unsigned TIMEOUT = EV_TIMEOUT;

    SocketEvent(EventLoop &_event_loop, Callback _callback)
        :event_loop(_event_loop), callback(_callback) {}

    SocketEvent(EventLoop &_event_loop, evutil_socket_t fd, unsigned events,
                Callback _callback)
        :SocketEvent(_event_loop, _callback) {
        Set(fd, events);
    }

    EventLoop &GetEventLoop() {
        return event_loop;
    }

    gcc_pure
    evutil_socket_t GetFd() const {
        return event.GetFd();
    }

    gcc_pure
    unsigned GetEvents() const {
        return event.GetEvents();
    }

    void Set(evutil_socket_t fd, unsigned events) {
        event.Set(event_loop, fd, events, EventCallback, this);
    }

    bool Add(const struct timeval *timeout=nullptr) {
        return event.Add(timeout);
    }

    bool Add(const struct timeval &timeout) {
        return event.Add(timeout);
    }

    void Delete() {
        event.Delete();
    }

    gcc_pure
    bool IsPending(unsigned events) const {
        return event.IsPending(events);
    }

    gcc_pure
    bool IsTimerPending() const {
        return event.IsTimerPending();
    }

private:
    static void EventCallback(gcc_unused evutil_socket_t fd, short events,
                              void *ctx) {
        auto &event = *(SocketEvent *)ctx;
        event.callback(events);
    }
};

#endif
