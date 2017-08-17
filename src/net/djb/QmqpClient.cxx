/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
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
