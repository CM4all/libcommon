/*
 * Copyright 2007-2021 CM4all GmbH
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

#pragma once

#include "NetstringClient.hxx"
#include "net/djb/NetstringHeader.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <list>
#include <forward_list>
#include <stdexcept>

#include <assert.h>

class QmqpClientHandler {
public:
	virtual void OnQmqpClientSuccess(StringView description) noexcept = 0;
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
	std::list<ConstBuffer<void>> request;

	QmqpClientHandler &handler;

public:
	QmqpClient(EventLoop &event_loop, QmqpClientHandler &_handler) noexcept
		:client(event_loop, 1024, *this),
		 handler(_handler) {}

	void Begin(StringView message, StringView sender) noexcept {
		assert(netstring_headers.empty());
		assert(request.empty());

		AppendNetstring(message);
		AppendNetstring(sender);
	}

	void AddRecipient(StringView recipient) noexcept {
		assert(!netstring_headers.empty());
		assert(!request.empty());

		AppendNetstring(recipient);
	}

	void Commit(FileDescriptor out_fd, FileDescriptor in_fd) noexcept;

private:
	void AppendNetstring(StringView value) noexcept;

	/* virtual methods from NetstringClientHandler */
	void OnNetstringResponse(AllocatedArray<uint8_t> &&payload) noexcept override;

	void OnNetstringError(std::exception_ptr error) noexcept override {
		/* forward to the QmqpClientHandler */
		handler.OnQmqpClientError(error);
	}
};
