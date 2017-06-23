/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SIGNAL_EVENT_HXX
#define SIGNAL_EVENT_HXX

#include "SocketEvent.hxx"
#include "util/BindMethod.hxx"

#include <assert.h>
#include <signal.h>

/**
 * Listen for signals delivered to this process, and then invoke a
 * callback.
 *
 * After constructing an instance, call Add() to add signals to listen
 * on.  When done, call Enable().  After that, Add() must not be
 * called again.
 */
class SignalEvent {
    int fd = -1;

    SocketEvent event;

    sigset_t mask;

    typedef BoundMethod<void(int)> Callback;
    const Callback callback;

public:
    SignalEvent(EventLoop &loop, Callback _callback);

    SignalEvent(EventLoop &loop, int signo, Callback _callback)
        :SignalEvent(loop, _callback) {
        Add(signo);
    }

    ~SignalEvent();

    bool IsDefined() const {
        return fd >= 0;
    }

    void Add(int signo) {
        assert(fd < 0);

        sigaddset(&mask, signo);
    }

    void Enable();
    void Disable();

private:
    void EventCallback(unsigned events);
};

#endif
