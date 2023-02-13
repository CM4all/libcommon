// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/SocketEvent.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Cancellable.hxx"
#include "util/IntrusiveList.hxx"
#include "AllocatedRequest.hxx"

#include <span>

enum class TranslationCommand : uint16_t;

namespace Translation::Server {

class Response;
class Handler;

class Connection : AutoUnlinkIntrusiveListHook
{
	friend struct IntrusiveListBaseHookTraits<Connection>;

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

	std::span<std::byte> output;

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
	bool OnPacket(TranslationCommand cmd,
		      std::span<const std::byte> payload) noexcept;

	/**
	 * @return false if this object has been destroyed
	 */
	bool TryWrite() noexcept;

	void OnSocketReady(unsigned events) noexcept;
};

} // namespace Translation::Server
