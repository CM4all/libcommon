// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "NetstringClient.hxx"
#include "net/djb/NetstringHeader.hxx"

#include <list>
#include <forward_list>
#include <stdexcept>
#include <string_view>

#include <assert.h>

class QmqpClientHandler {
public:
	virtual void OnQmqpClientSuccess(std::string_view description) noexcept = 0;
	virtual void OnQmqpClientError(std::exception_ptr error) noexcept = 0;
};

class QmqpClientError : std::runtime_error {
public:
	template<typename W>
	QmqpClientError(W &&what_arg) noexcept
		:std::runtime_error(std::forward<W>(what_arg)) {}
};

class QmqpClientTemporaryFailure final : QmqpClientError {
public:
	template<typename W>
	QmqpClientTemporaryFailure(W &&what_arg) noexcept
		:QmqpClientError(std::forward<W>(what_arg)) {}
};

class QmqpClientPermanentFailure final : QmqpClientError {
public:
	template<typename W>
	QmqpClientPermanentFailure(W &&what_arg) noexcept
		:QmqpClientError(std::forward<W>(what_arg)) {}
};

/**
 * A client which sends an email to a QMQP server and receives its
 * response.
 */
class QmqpClient final : NetstringClientHandler {
	NetstringClient client;

	std::forward_list<NetstringHeader> netstring_headers;
	std::list<std::span<const std::byte>> request;

	QmqpClientHandler &handler;

public:
	QmqpClient(EventLoop &event_loop, QmqpClientHandler &_handler) noexcept
		:client(event_loop, 1024, *this),
		 handler(_handler) {}

	void Begin(std::string_view message, std::string_view sender) noexcept {
		assert(netstring_headers.empty());
		assert(request.empty());

		AppendNetstring(message);
		AppendNetstring(sender);
	}

	void AddRecipient(std::string_view recipient) noexcept {
		assert(!netstring_headers.empty());
		assert(!request.empty());

		AppendNetstring(recipient);
	}

	void Commit(FileDescriptor out_fd, FileDescriptor in_fd) noexcept;

private:
	void AppendNetstring(std::string_view value) noexcept;

	/* virtual methods from NetstringClientHandler */
	void OnNetstringResponse(AllocatedArray<std::byte> &&payload) noexcept override;

	void OnNetstringError(std::exception_ptr error) noexcept override {
		/* forward to the QmqpClientHandler */
		handler.OnQmqpClientError(error);
	}
};
