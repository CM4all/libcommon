/*
 * author: Max Kellermann <mk@cm4all.com>
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
