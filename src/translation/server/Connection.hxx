/*
 * Copyright 2007-2019 Content Management AG
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

#include "event/SocketEvent.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Cancellable.hxx"
#include "AllocatedRequest.hxx"

enum class TranslationCommand : uint16_t;
template<typename T> struct ConstBuffer;

namespace Translation::Server {

class Response;
class Listener;
class Handler;

class Connection
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>
{
	Listener &listener;
	Handler &handler;

	const UniqueSocketDescriptor fd;
	SocketEvent event;

	enum class State {
		INIT,
		REQUEST,
		PROCESSING,
		RESPONSE,
	} state;

	DynamicFifoBuffer<uint8_t> input;

	AllocatedRequest request;

	/**
         * If this is set, then our #handler is currently handling the
         * #request.
	 */
	CancellablePointer cancel_ptr{nullptr};

	uint8_t *response;

	WritableBuffer<uint8_t> output;

public:
	Connection(EventLoop &event_loop,
		   Listener &_listener, Handler &_handler,
		   UniqueSocketDescriptor &&_fd) noexcept;
	~Connection() noexcept;

	/**
	 * @return false if this object has been destroyed
	 */
	bool SendResponse(Response &&response) noexcept;

private:
	bool TryRead() noexcept;
	bool OnReceived() noexcept;
	bool OnPacket(TranslationCommand cmd, ConstBuffer<void> payload) noexcept;

	/**
	 * @return false if this object has been destroyed
	 */
	bool TryWrite() noexcept;

	void OnSocketReady(unsigned events) noexcept;
};

} // namespace Translation::Server
