/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef QMQP_CLIENT_HXX
#define QMQP_CLIENT_HXX

#include "NetstringClient.hxx"
#include "NetstringHeader.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <list>
#include <forward_list>

#include <assert.h>

class QmqpClientHandler {
public:
    virtual void OnQmqpClientSuccess(StringView description) = 0;
    virtual void OnQmqpClientError(std::exception_ptr error) = 0;
};

class QmqpClientError : std::runtime_error {
public:
    template<typename W>
    QmqpClientError(W &&what_arg)
        :std::runtime_error(std::forward<W>(what_arg)) {}
};

class QmqpClientTemporaryFailure final : QmqpClientError {
public:
    template<typename W>
    QmqpClientTemporaryFailure(W &&what_arg)
        :QmqpClientError(std::forward<W>(what_arg)) {}
};

class QmqpClientPermanentFailure final : QmqpClientError {
public:
    template<typename W>
    QmqpClientPermanentFailure(W &&what_arg)
        :QmqpClientError(std::forward<W>(what_arg)) {}
};

/**
 * A client which sends an email to a QMQP server and receives its
 * response.
 */
class QmqpClient final : NetstringClientHandler {
    NetstringClient client;

    std::forward_list<NetstringHeader> netstring_headers;
    std::list<ConstBuffer<void>> request;

    QmqpClientHandler &handler;

public:
    QmqpClient(EventLoop &event_loop, QmqpClientHandler &_handler)
        :client(event_loop, 1024, *this),
         handler(_handler) {}

    void Begin(StringView message, StringView sender) {
        assert(netstring_headers.empty());
        assert(request.empty());

        AppendNetstring(message);
        AppendNetstring(sender);
    }

    void AddRecipient(StringView recipient) {
        assert(!netstring_headers.empty());
        assert(!request.empty());

        AppendNetstring(recipient);
    }

    void Commit(int out_fd, int in_fd);

private:
    void AppendNetstring(StringView value);

    /* virtual methods from NetstringClientHandler */
    void OnNetstringResponse(AllocatedArray<uint8_t> &&payload) override;

    void OnNetstringError(std::exception_ptr error) override {
        /* forward to the QmqpClientHandler */
        handler.OnQmqpClientError(error);
    }
};

#endif
