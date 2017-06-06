/*
 * A server that receives netstrings
 * (http://cr.yp.to/proto/netstrings.txt) from its clients and
 * responds with another netstring.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "NetstringServer.hxx"
#include "util/ConstBuffer.hxx"

#include <stdexcept>

#include <string.h>

static constexpr timeval busy_timeout{5, 0};

NetstringServer::NetstringServer(EventLoop &event_loop,
                                 UniqueSocketDescriptor &&_fd)
    :fd(std::move(_fd)),
     event(event_loop, fd.Get(), EV_READ|EV_PERSIST,
           BIND_THIS_METHOD(OnEvent)),
     input(16 * 1024 * 1024) {
    event.Add(busy_timeout);
}

NetstringServer::~NetstringServer()
{
    event.Delete();
}

bool
NetstringServer::SendResponse(const void *data, size_t size)
try {
    std::list<ConstBuffer<void>> list{{data, size}};
    generator(list);
    for (const auto &i : list)
        write.Push(i.data, i.size);

    switch (write.Write(fd.Get())) {
    case MultiWriteBuffer::Result::MORE:
        throw std::runtime_error("short write");

    case MultiWriteBuffer::Result::FINISHED:
        return true;
    }

    assert(false);
    gcc_unreachable();
} catch (const std::runtime_error &) {
    OnError(std::current_exception());
    return false;
}

bool
NetstringServer::SendResponse(const char *data)
{
    return SendResponse((const void *)data, strlen(data));
}

void
NetstringServer::OnEvent(unsigned events)
try {
    if (events & EV_TIMEOUT) {
        OnDisconnect();
        return;
    }

    switch (input.Receive(fd.Get())) {
    case NetstringInput::Result::MORE:
        event.Add(&busy_timeout);
        break;

    case NetstringInput::Result::CLOSED:
        OnDisconnect();
        break;

    case NetstringInput::Result::FINISHED:
        event.Delete();
        OnRequest(std::move(input.GetValue()));
        break;
    }
} catch (...) {
    OnError(std::current_exception());
}
