// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "PipeEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <span>

class PipeLineReaderHandler {
public:
	/**
	 * @return true to continue reading lines, false if the
	 * #PipeLineReader has been destroyed
	 */
	virtual bool OnPipeLine(std::span<char> line) noexcept = 0;
	virtual void OnPipeEnd() noexcept = 0;
};

/**
 * Read text lines from a (non-blocking) pipe.  Whenever a newline
 * character is found, the line (without trailing NL/CR characters) is
 * passed to the callback.
 */
class PipeLineReader {
	PipeEvent event;

	PipeLineReaderHandler &handler;

	StaticFifoBuffer<char, 8192> buffer;

public:
	/**
	 * @param callback this function will be invoked for every
	 * received line, and again with a nullptr parameter at the
	 * end of the pipe; it returns false if the #PipeLineReader
	 * has been destroyed inside the callback
	 */
	PipeLineReader(EventLoop &event_loop,
		       UniqueFileDescriptor fd,
		       PipeLineReaderHandler &_handler) noexcept
		:event(event_loop, BIND_THIS_METHOD(OnPipeReadable),
		       fd.Release()),
		 handler(_handler)
	{
		event.ScheduleRead();
	}

	~PipeLineReader() noexcept {
		event.Close();
	}

	EventLoop &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	/**
	 * Attempt to read again, and pass all data to the callback.  If
	 * the last line isn't finalized with a newline character, it is
	 * passed to the callback as well.  After this method returns, the
	 * buffer is empty.  Call this when the child process exits, to
	 * ensure that everything in the pipe is handled.
	 */
	void Flush() noexcept {
		TryRead(true);
	}

private:
	void TryRead(bool flush) noexcept;

	void OnPipeReadable(unsigned) noexcept {
		TryRead(false);
	}
};
