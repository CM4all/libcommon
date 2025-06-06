// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/FileDescriptor.hxx"
#include "io/MultiWriteBuffer.hxx"
#include "net/djb/NetstringGenerator.hxx"
#include "net/djb/NetstringInput.hxx"
#include "event/SocketEvent.hxx"
#include "event/CoarseTimerEvent.hxx"

#include <list>
#include <exception>

class NetstringClientHandler {
public:
	virtual void OnNetstringResponse(AllocatedArray<std::byte> &&payload) noexcept = 0;
	virtual void OnNetstringError(std::exception_ptr error) noexcept = 0;
};

/**
 * A client that sends a netstring
 * (http://cr.yp.to/proto/netstrings.txt) and receives another
 * netstring.
 *
 * To use it, first construct an instance, then call Request() with a
 * socket (or two pipes) that are already connected to the QMQP
 * server.
 *
 * It is not possible to reuse an instance for a second email.
 */
class NetstringClient final {
	FileDescriptor out_fd = FileDescriptor::Undefined();
	FileDescriptor in_fd = FileDescriptor::Undefined();

	SocketEvent event;
	CoarseTimerEvent timeout_event;

	NetstringGenerator generator;
	MultiWriteBuffer write;

	NetstringInput input;

	NetstringClientHandler &handler;

public:
	NetstringClient(EventLoop &event_loop, size_t max_size,
			NetstringClientHandler &_handler) noexcept;
	~NetstringClient() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	/**
	 * Start sending the request.  This method may be called only
	 * once.
	 *
	 * @param _out_fd a connected socket (or a pipe) for sending data
	 * to the QMQP server
	 * @param _in_fd a connected socket (or a pipe) for receiving data
	 * from the QMQP server (may be equal to #_out_fd)
	 * @param data a list of request data chunks which will be
	 * concatenated, without the Netstring header/trailer; the memory
	 * regions being pointed to must remain valid until the whole
	 * request has been sent (i.e. until the #NetstringClientHandler
	 * has been invoked)
	 */
	void Request(FileDescriptor _out_fd, FileDescriptor _in_fd,
		     std::list<std::span<const std::byte>> &&data) noexcept;

private:
	void OnEvent(unsigned events) noexcept;
	void OnTimeout() noexcept;
};
