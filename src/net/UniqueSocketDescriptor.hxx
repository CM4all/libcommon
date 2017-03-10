/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX
#define UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX

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
class UniqueSocketDescriptor {
    int fd = -1;

public:
    UniqueSocketDescriptor() = default;

    explicit UniqueSocketDescriptor(int _fd):fd(_fd) {
        assert(fd >= 0);
    }

    UniqueSocketDescriptor(UniqueSocketDescriptor &&other)
        :fd(std::exchange(other.fd, -1)) {}

    ~UniqueSocketDescriptor();

    UniqueSocketDescriptor &operator=(UniqueSocketDescriptor &&src) {
        std::swap(fd, src.fd);
        return *this;
    }

    bool IsDefined() const {
        return fd >= 0;
    }

    int Get() const {
        assert(IsDefined());

        return fd;
    }

    bool operator==(const UniqueSocketDescriptor &other) const {
        return fd == other.fd;
    }

    void Close();

    int Steal() {
        assert(IsDefined());

        return std::exchange(fd, -1);
    }

    /**
     * @return false on error (with errno set)
     */
    bool CreateNonBlock(int domain, int type, int protocol);

    bool Bind(SocketAddress address);

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

    /**
     * @return false on error (with errno set)
     */
    bool Connect(const SocketAddress address);

    int GetError();

    gcc_pure
    StaticSocketAddress GetLocalAddress() const;
};

#endif
