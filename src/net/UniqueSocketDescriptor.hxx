/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX
#define UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX

#include "SocketDescriptor.hxx"

#include <inline/compiler.h>

#include <algorithm>
#include <utility>

#include <assert.h>
#include <stddef.h>

class SocketAddress;
class StaticSocketAddress;

/**
 * Wrapper for a socket file descriptor.
 */
class UniqueSocketDescriptor : public SocketDescriptor {
public:
    UniqueSocketDescriptor()
        :SocketDescriptor(SocketDescriptor::Undefined()) {}

    explicit UniqueSocketDescriptor(SocketDescriptor _fd)
        :SocketDescriptor(_fd) {}
    explicit UniqueSocketDescriptor(FileDescriptor _fd)
        :SocketDescriptor(_fd) {}
    explicit UniqueSocketDescriptor(int _fd):SocketDescriptor(_fd) {}

    UniqueSocketDescriptor(UniqueSocketDescriptor &&other)
        :SocketDescriptor(std::exchange(other.fd, -1)) {}

    ~UniqueSocketDescriptor() {
        if (IsDefined())
            Close();
    }

    UniqueSocketDescriptor &operator=(UniqueSocketDescriptor &&src) {
        std::swap(fd, src.fd);
        return *this;
    }

    bool operator==(const UniqueSocketDescriptor &other) const {
        return fd == other.fd;
    }

    bool SetOption(int level, int name, const void *value, size_t size);

    bool SetBoolOption(int level, int name, bool _value) {
        const int value = _value;
        return SetOption(level, name, &value, sizeof(value));
    }

    bool SetReuseAddress(bool value=true);
    bool SetReusePort(bool value=true);

    bool SetTcpDeferAccept(const int &seconds);
    bool SetV6Only(bool value);

    /**
     * Setter for SO_BINDTODEVICE.
     */
    bool SetBindToDevice(const char *name);

    bool SetTcpFastOpen(int qlen=16);

    /**
     * @return an "undefined" instance on error
     */
    UniqueSocketDescriptor Accept(StaticSocketAddress &address) const;

    int GetError();

    gcc_pure
    StaticSocketAddress GetLocalAddress() const;
};

#endif
