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
 *
 * To use it, first construct an instance, then call Request() with a
 * socket (or two pipes) that are already connected to the QMQP
 * server.
 *
 * It is not possible to reuse an instance for a second email.
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

    /**
     * Start sending the request.  This method may be called only
     * once.
     *
     * @param _out_fd a connected socket (or a pipe) for sending data
     * to the QMQP server
     * @param _in_fd a connected socket (or a pipe) for receiving data
     * from the QMQP server (may be equal to #_out_fd)
     * @param data a list of request data chunks which will be
     * concatenated, without the Netstring header/trailer; the memory
     * regions being pointed to must remain valid until the whole
     * request has been sent (i.e. until the #NetstringClientHandler
     * has been invoked)
     */
    void Request(int _out_fd, int _in_fd,
                 std::list<ConstBuffer<void>> &&data);

private:
    void OnEvent(unsigned events);
};

#endif
