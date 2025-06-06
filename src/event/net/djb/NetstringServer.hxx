// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/MultiWriteBuffer.hxx"
#include "net/djb/NetstringInput.hxx"
#include "net/djb/NetstringGenerator.hxx"
#include "event/SocketEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/SpanCast.hxx"

#include <exception>
#include <cstddef>
#include <span>

class UniqueSocketDescriptor;

/**
 * A server that receives netstrings
 * (http://cr.yp.to/proto/netstrings.txt) from its clients and
 * responds with another netstring.
 */
class NetstringServer {
	SocketEvent event;
	CoarseTimerEvent timeout_event;

	NetstringInput input;
	NetstringGenerator generator;
	MultiWriteBuffer write;

public:
	NetstringServer(EventLoop &event_loop,
			UniqueSocketDescriptor _fd,
			size_t max_size=16*1024*1024) noexcept;
	~NetstringServer() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

protected:
	SocketDescriptor GetSocket() const noexcept {
		return event.GetSocket();
	}

	bool SendResponse(std::span<const std::byte> response) noexcept;

	bool SendResponse(std::string_view response) noexcept {
		return SendResponse(AsBytes(response));
	}

	/**
	 * A netstring has been received.
	 *
	 * @param payload the netstring value; for the implementation's
	 * convenience, the netstring is writable
	 */
	virtual void OnRequest(AllocatedArray<std::byte> &&payload) = 0;
	virtual void OnError(std::exception_ptr ep) noexcept = 0;
	virtual void OnDisconnect() noexcept = 0;

private:
	bool IsRequestReceived() const noexcept {
		/* the timeout gets canceled as soon as the request
                   has been fully received, therefore we can use this
                   field here */
		return !timeout_event.IsPending();
	}

	void OnEvent(unsigned events) noexcept;
	void OnTimeout() noexcept;
};
