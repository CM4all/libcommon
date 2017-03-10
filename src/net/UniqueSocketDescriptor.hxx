/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX
#define UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX

#include "SocketDescriptor.hxx"

#include <algorithm>
#include <utility>

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

    /**
     * @return an "undefined" instance on error
     */
    UniqueSocketDescriptor AcceptNonBlock(StaticSocketAddress &address) const;
};

#endif
