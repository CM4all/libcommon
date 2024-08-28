// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "event/DeferEvent.hxx"
#include "util/DisposableBuffer.hxx"

class UniqueFileDescriptor;

namespace Was {

class SimpleOutputHandler {
public:
	virtual void OnWasOutputError(std::exception_ptr error) noexcept = 0;
};

class SimpleOutput final {
	PipeEvent event;
	DeferEvent defer_write;

	SimpleOutputHandler &handler;

	DisposableBuffer buffer;

	std::size_t position;

public:
	SimpleOutput(EventLoop &event_loop, UniqueFileDescriptor pipe,
		     SimpleOutputHandler &_handler) noexcept;
	~SimpleOutput() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Close() noexcept {
		event.Close();
		defer_write.Cancel();
	}

	bool IsActive() const noexcept {
		return buffer;
	}

	void Activate(DisposableBuffer _buffer) noexcept;

	/**
	 * Set the "position" field to zero to allow calling Stop()
	 * without Activate(), in cases where there is no request
	 * body.
	 */
	void ResetPosition() noexcept {
		position = 0;
	}

	/**
	 * Handle a STOP command.  Returns the number of bytes already
	 * written to the pipe.  This may be called even after writing
	 * has completed (because the `position` field does not get
	 * cleared).
	 */
	std::size_t Stop() noexcept {
		buffer = {};
		event.Cancel();
		return position;
	}

private:
	FileDescriptor GetPipe() const noexcept {
		return event.GetFileDescriptor();
	}

	void TryWrite();
	void OnDeferredWrite() noexcept;
	void OnPipeReady(unsigned events) noexcept;
};

} // namespace Was
