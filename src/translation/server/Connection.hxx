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

#include "event/SocketEvent.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Cancellable.hxx"
#include "util/IntrusiveList.hxx"
#include "AllocatedRequest.hxx"

enum class TranslationCommand : uint16_t;
template<typename T> struct ConstBuffer;

namespace Translation::Server {

class Response;
class Handler;

class Connection : AutoUnlinkIntrusiveListHook
{
	friend class IntrusiveList<Connection>;

	Handler &handler;

	SocketEvent event;

	enum class State {
		INIT,
		REQUEST,
		PROCESSING,
		RESPONSE,
	} state;

	DynamicFifoBuffer<std::byte> input;

	AllocatedRequest request;

	/**
         * If this is set, then our #handler is currently handling the
         * #request.
	 */
	CancellablePointer cancel_ptr{nullptr};

	std::byte *response;

	WritableBuffer<std::byte> output;

public:
	Connection(EventLoop &event_loop,
		   Handler &_handler,
		   UniqueSocketDescriptor &&_fd) noexcept;
	~Connection() noexcept;

	/**
	 * @return false if this object has been destroyed
	 */
	bool SendResponse(Response &&response) noexcept;

private:
	void Destroy() noexcept {
		delete this;
	}

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
