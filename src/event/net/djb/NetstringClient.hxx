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

#include "io/FileDescriptor.hxx"
#include "io/MultiWriteBuffer.hxx"
#include "net/djb/NetstringGenerator.hxx"
#include "net/djb/NetstringInput.hxx"
#include "event/SocketEvent.hxx"
#include "event/CoarseTimerEvent.hxx"

#include <list>
#include <exception>

template<typename T> struct ConstBuffer;

class NetstringClientHandler {
public:
	virtual void OnNetstringResponse(AllocatedArray<uint8_t> &&payload) noexcept = 0;
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
		     std::list<ConstBuffer<void>> &&data) noexcept;

private:
	void OnEvent(unsigned events) noexcept;
	void OnTimeout() noexcept;
};
