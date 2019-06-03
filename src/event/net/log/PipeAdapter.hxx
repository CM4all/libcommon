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

#include "net/SocketDescriptor.hxx"
#include "net/log/Datagram.hxx"
#include "event/PipeLineReader.hxx"
#include "util/TokenBucket.hxx"

namespace Net {
namespace Log {

/**
 * Reads lines from a pipe and sends them to a Pond server.  This can
 * be useful as an adapter for a child process's stderr.
 *
 * If the pipe ends or fails, there is no callback/notification.  This
 * class just unregisters the event and stops operating.
 */
class PipeAdapter {
	PipeLineReader line_reader;

	SocketDescriptor socket;

	Datagram datagram;

	TokenBucket token_bucket;

	double rate_limit = -1, burst;

public:
	/**
	 * @param _pipe the pipe this class will read lines from
	 * @param _socket a datagram socket connected to a Pond server
	 * (owned by caller)
	 */
	PipeAdapter(EventLoop &event_loop, UniqueFileDescriptor _pipe,
		    SocketDescriptor _socket) noexcept
		:line_reader(event_loop, std::move(_pipe),
			     BIND_THIS_METHOD(OnLine)),
		 socket(_socket) {}

	EventLoop &GetEventLoop() const noexcept {
		return line_reader.GetEventLoop();
	}

	/**
	 * Returns a reference to the #Datagram instance.  The caller
	 * may modify it.  New pointed-to data is owned by the caller
	 * and must be kept alive/valid as long as this #PipeAdapter
	 * lives.
	 */
	Datagram &GetDatagram() noexcept {
		return datagram;
	}

	void SetRateLimit(double _rate_limit, double _burst) noexcept {
		rate_limit = _rate_limit;
		burst = _burst;
	}

	void Flush() noexcept {
		line_reader.Flush();
	}

private:
	bool OnLine(WritableBuffer<char> line) noexcept;
};

}}
