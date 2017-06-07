/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "NetstringClient.hxx"
#include "util/ConstBuffer.hxx"

static constexpr timeval send_timeout{10, 0};
static constexpr timeval recv_timeout{60, 0};
static constexpr timeval busy_timeout{5, 0};

NetstringClient::NetstringClient(EventLoop &event_loop, size_t max_size,
                                 NetstringClientHandler &_handler)
    :event(event_loop, BIND_THIS_METHOD(OnEvent)),
     input(max_size), handler(_handler) {}

NetstringClient::~NetstringClient()
{
    if (out_fd >= 0 || in_fd >= 0)
        event.Delete();

    if (out_fd >= 0)
        close(out_fd);

    if (in_fd >= 0 && in_fd != out_fd)
        close(in_fd);
}

void
NetstringClient::Request(int _out_fd, int _in_fd,
                         std::list<ConstBuffer<void>> &&data)
{
    assert(in_fd < 0);
    assert(out_fd < 0);
    assert(_in_fd >= 0);
    assert(_out_fd >= 0);

    out_fd = _out_fd;
    in_fd = _in_fd;

    generator(data);
    for (const auto &i : data)
        write.Push(i.data, i.size);

    event.Set(out_fd, SocketEvent::WRITE|SocketEvent::PERSIST);
    event.Add(send_timeout);
}

void
NetstringClient::OnEvent(unsigned events)
try {
    if (events & SocketEvent::TIMEOUT) {
        throw std::runtime_error("Connect timeout");
    } else if (events & SocketEvent::WRITE) {
        switch (write.Write(out_fd)) {
        case MultiWriteBuffer::Result::MORE:
            event.Add(&send_timeout);
            break;

        case MultiWriteBuffer::Result::FINISHED:
            event.Delete();
            event.Set(in_fd, SocketEvent::READ|SocketEvent::PERSIST);
            event.Add(recv_timeout);
            break;
        }
    } else if (events & SocketEvent::READ) {
        switch (input.Receive(in_fd)) {
        case NetstringInput::Result::MORE:
            event.Add(&busy_timeout);
            break;

        case NetstringInput::Result::CLOSED:
            throw std::runtime_error("Connection closed prematurely");

        case NetstringInput::Result::FINISHED:
            event.Delete();
            handler.OnNetstringResponse(std::move(input.GetValue()));
            break;
        }
    }
} catch (...) {
    handler.OnNetstringError(std::current_exception());
}
