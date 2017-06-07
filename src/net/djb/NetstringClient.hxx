/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef NETSTRING_CLIENT_HXX
#define NETSTRING_CLIENT_HXX

#include "NetstringGenerator.hxx"
#include "NetstringInput.hxx"
#include "io/MultiWriteBuffer.hxx"
#include "event/SocketEvent.hxx"
#include "util/ConstBuffer.hxx"

#include <list>
#include <exception>

template<typename T> struct ConstBuffer;

class NetstringClientHandler {
public:
    virtual void OnNetstringResponse(AllocatedArray<uint8_t> &&payload) = 0;
    virtual void OnNetstringError(std::exception_ptr error) = 0;
};

/**
 * A client that sends a netstring
 * (http://cr.yp.to/proto/netstrings.txt) and receives another
 * netstring.
 */
class NetstringClient final {
    int out_fd = -1, in_fd = -1;

    SocketEvent event;

    NetstringGenerator generator;
    MultiWriteBuffer write;

    NetstringInput input;

    NetstringClientHandler &handler;

public:
    NetstringClient(EventLoop &event_loop, size_t max_size,
                    NetstringClientHandler &_handler);
    ~NetstringClient();

    void Request(int _out_fd, int _in_fd,
                 std::list<ConstBuffer<void>> &&data);

private:
    void OnEvent(unsigned events);
};

#endif
