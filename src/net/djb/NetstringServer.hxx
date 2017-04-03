/*
 * A server that receives netstrings
 * (http://cr.yp.to/proto/netstrings.txt) from its clients and
 * responds with another netstring.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef NETSTRING_SERVER_HXX
#define NETSTRING_SERVER_HXX

#include "NetstringInput.hxx"
#include "NetstringGenerator.hxx"
#include "io/MultiWriteBuffer.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "event/SocketEvent.hxx"

#include <exception>
#include <cstddef>

class NetstringServer {
    UniqueSocketDescriptor fd;

    SocketEvent event;

    NetstringInput input;
    NetstringGenerator generator;
    MultiWriteBuffer write;

public:
    NetstringServer(EventLoop &event_loop, UniqueSocketDescriptor &&_fd);
    ~NetstringServer();

protected:
    int GetFD() const {
        return fd.Get();
    }

    bool SendResponse(const void *data, size_t size);
    bool SendResponse(const char *data);

    /**
     * A netstring has been received.
     *
     * @param payload the netstring value; for the implementation's
     * convenience, the netstring is writable
     */
    virtual void OnRequest(AllocatedArray<uint8_t> &&payload) = 0;
    virtual void OnError(std::exception_ptr ep) = 0;
    virtual void OnDisconnect() = 0;

private:
    void OnEvent(unsigned events);
};

#endif
