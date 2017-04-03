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
    :out_fd(-1), in_fd(-1),
     event(event_loop, BIND_THIS_METHOD(OnEvent)),
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

    event.Set(out_fd, EV_WRITE|EV_TIMEOUT|EV_PERSIST);
    event.Add(send_timeout);
}

void
NetstringClient::OnEvent(unsigned events)
try {
    if (events & EV_TIMEOUT) {
        throw std::runtime_error("Connect timeout");
    } else if (events & EV_WRITE) {
        switch (write.Write(out_fd)) {
        case MultiWriteBuffer::Result::MORE:
            event.Add(&send_timeout);
            break;

        case MultiWriteBuffer::Result::FINISHED:
            event.Delete();
            event.Set(in_fd, EV_READ|EV_TIMEOUT|EV_PERSIST);
            event.Add(recv_timeout);
            break;
        }
    } else if (events & EV_READ) {
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
