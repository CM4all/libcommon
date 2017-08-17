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

#ifndef SOCKET_WRAPPER_HXX
#define SOCKET_WRAPPER_HXX

#include "io/FdType.hxx"
#include "event/SocketEvent.hxx"
#include "net/SocketDescriptor.hxx"

#include "util/Compiler.h"

#include <sys/types.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

template<typename T> class ForeignFifoBuffer;

class SocketHandler {
public:
    /**
     * The socket is ready for reading.
     *
     * @return false when the socket has been closed
     */
    virtual bool OnSocketRead() = 0;

    /**
     * The socket is ready for writing.
     *
     * @return false when the socket has been closed
     */
    virtual bool OnSocketWrite() = 0;

    /**
     * @return false when the socket has been closed
     */
    virtual bool OnSocketTimeout() = 0;
};

class SocketWrapper {
    SocketDescriptor fd;
    FdType fd_type;

    SocketEvent read_event, write_event;

    SocketHandler &handler;

public:
    SocketWrapper(EventLoop &event_loop, SocketHandler &_handler)
        :read_event(event_loop, BIND_THIS_METHOD(ReadEventCallback)),
         write_event(event_loop, BIND_THIS_METHOD(WriteEventCallback)),
         handler(_handler) {}

    SocketWrapper(const SocketWrapper &) = delete;

    EventLoop &GetEventLoop() {
        return read_event.GetEventLoop();
    }

    void Init(SocketDescriptor _fd, FdType _fd_type);

    /**
     * Move the socket from another #SocketWrapper instance.  This
     * disables scheduled events.
     */
    void Init(SocketWrapper &&src);

    /**
     * Shut down the socket gracefully, allowing the TCP stack to
     * complete all pending transfers.  If you call Close() without
     * Shutdown(), it may reset the connection and discard pending
     * data.
     */
    void Shutdown();

    void Close();

    /**
     * Just like Close(), but do not actually close the
     * socket.  The caller is responsible for closing the socket (or
     * scheduling it for reuse).
     */
    void Abandon();

    /**
     * Returns the socket descriptor and calls socket_wrapper_abandon().
     */
    int AsFD();

    bool IsValid() const {
        return fd.IsDefined();
    }

    int GetFD() const {
        return fd.Get();
    }

    FdType GetType() const {
        return fd_type;
    }

    void ScheduleRead(const struct timeval *timeout) {
        assert(IsValid());

        if (timeout == nullptr && read_event.IsTimerPending())
            /* work around libevent bug: event_add() should disable the
               timeout if tv==nullptr, but in fact it does not; workaround:
               delete the whole event first, then re-add it */
            read_event.Delete();

        read_event.Add(timeout);
    }

    void UnscheduleRead() {
        read_event.Delete();
    }

    void ScheduleWrite(const struct timeval *timeout) {
        assert(IsValid());

        if (timeout == nullptr && write_event.IsTimerPending())
            /* work around libevent bug: event_add() should disable the
               timeout if tv==nullptr, but in fact it does not; workaround:
               delete the whole event first, then re-add it */
            write_event.Delete();

        write_event.Add(timeout);
    }

    void UnscheduleWrite() {
        write_event.Delete();
    }

    gcc_pure
    bool IsReadPending() const {
        return read_event.IsPending(SocketEvent::READ);
    }

    gcc_pure
    bool IsWritePending() const {
        return write_event.IsPending(SocketEvent::WRITE);
    }

    ssize_t ReadToBuffer(ForeignFifoBuffer<uint8_t> &buffer);

    gcc_pure
    bool IsReadyForWriting() const;

    ssize_t Write(const void *data, size_t length);

    ssize_t WriteV(const struct iovec *v, size_t n);

    ssize_t WriteFrom(int other_fd, FdType other_fd_type,
                      size_t length);

private:
    void ReadEventCallback(unsigned events);
    void WriteEventCallback(unsigned events);
};

#endif
