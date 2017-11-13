/*
 * Copyright 2007-2017 Content Management AG
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

#include "SocketEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/BindMethod.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "util/WritableBuffer.hxx"

/**
 * Read text lines from a (non-blocking) pipe.  Whenever a newline
 * character is found, the line (without trailing NL/CR characters) is
 * passed to the callback.
 */
class PipeLineReader {
	UniqueFileDescriptor fd;
	SocketEvent event;

	StaticFifoBuffer<char, 8192> buffer;

	typedef BoundMethod<bool(WritableBuffer<char> line)> Callback;
	const Callback callback;

public:
	/**
	 * @param callback this function will be invoked for every
	 * received line, and again with a nullptr parameter at the
	 * end of the pipe; it returns false if the #PipeLineReader
	 * has been destroyed inside the callback
	 */
	PipeLineReader(EventLoop &event_loop,
		       UniqueFileDescriptor _fd,
		       Callback _callback)
		:fd(std::move(_fd)),
		 event(event_loop, fd.Get(), SocketEvent::READ|SocketEvent::PERSIST,
		       BIND_THIS_METHOD(OnPipeReadable)),
		 callback(_callback) {
		event.Add();
	}

	~PipeLineReader() {
		event.Delete();
	}

	/**
	 * Attempt to read again, and pass all data to the callback.  If
	 * the last line isn't finalized with a newline character, it is
	 * passed to the callback as well.  After this method returns, the
	 * buffer is empty.  Call this when the child process exits, to
	 * ensure that everything in the pipe is handled.
	 */
	void Flush() {
		TryRead(true);
	}

private:
	void TryRead(bool flush);

	void OnPipeReadable(unsigned) {
		TryRead(false);
	}
};
