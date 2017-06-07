/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "QmqpClient.hxx"

void
QmqpClient::AppendNetstring(StringView value)
{
    netstring_headers.emplace_front();
    auto &g = netstring_headers.front();
    request.emplace_back(g(value.size).ToVoid());
    request.emplace_back(value.ToVoid());
    request.emplace_back(",", 1);
}

void
QmqpClient::Commit(int out_fd, int in_fd)
{
    assert(!netstring_headers.empty());
    assert(!request.empty());

    client.Request(out_fd, in_fd, std::move(request));
}

void
QmqpClient::OnNetstringResponse(AllocatedArray<uint8_t> &&_payload)
try {
    StringView payload((const char *)&_payload.front(), _payload.size());

    if (!payload.IsEmpty()) {
        switch (payload[0]) {
        case 'K':
            // success
            payload.pop_front();
            handler.OnQmqpClientSuccess(payload);
            return;

        case 'Z':
            // temporary failure
            payload.pop_front();
            throw QmqpClientTemporaryFailure(std::string(payload.data,
                                                         payload.size));

        case 'D':
            // permanent failure
            payload.pop_front();
            throw QmqpClientPermanentFailure(std::string(payload.data,
                                                         payload.size));
        }
    }

    throw QmqpClientError("Malformed QMQP response");
} catch (...) {
    handler.OnQmqpClientError(std::current_exception());
}
