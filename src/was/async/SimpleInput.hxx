// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "event/DeferEvent.hxx"

#include <cstddef>
#include <exception> // for std::exception_ptr
#include <memory>

class UniqueFileDescriptor;
class DisposableBuffer;

namespace Was {

class Buffer;

class SimpleInputHandler {
public:
	virtual void OnWasInput(DisposableBuffer input) noexcept = 0;
	virtual void OnWasInputHangup() noexcept = 0;
	virtual void OnWasInputError(std::exception_ptr error) noexcept = 0;
};

class SimpleInput final {
	PipeEvent event;
	DeferEvent defer_read;

	SimpleInputHandler &handler;

	std::unique_ptr<Buffer> buffer;

	/**
	 * The number of body bytes already read from the pipe.  Unlike
	 * Buffer::GetFill(), this remains valid after #buffer has been
	 * released, so that a #WAS_COMMAND_PREMATURE packet arriving
	 * later can still be validated.
	 */
	std::size_t position = 0;

public:
	SimpleInput(EventLoop &event_loop, UniqueFileDescriptor pipe,
		    SimpleInputHandler &_handler) noexcept;
	~SimpleInput() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Close() noexcept {
		event.Close();
		defer_read.Cancel();
	}

	bool IsActive() const noexcept {
		return buffer != nullptr;
	}

	void Activate() noexcept;

	/**
	 * Set the "position" field to zero.  This must be called when a
	 * new request/response cycle begins, because #position is only
	 * meaningful for the current body; a stale value would make the
	 * check in Premature() compare against the previous body.
	 */
	void ResetPosition() noexcept {
		position = 0;
	}

	bool SetLength(std::size_t length) noexcept;

	DisposableBuffer CheckComplete() noexcept;

	/**
	 * Throws on error.
	 */
	void Premature(std::size_t nbytes);

	/**
	 * Discard the rest of the body and deactivate this object, so
	 * the pipe can be reused for the next request.
	 *
	 * This is only possible if the total length has been announced
	 * with #WAS_COMMAND_LENGTH; if it has not, only the peer can
	 * tell us how much it has written, by sending
	 * #WAS_COMMAND_PREMATURE.
	 *
	 * Throws on error.
	 *
	 * @return false if the total length is unknown
	 */
	bool Discard();

private:
	/**
	 * Hand out the buffer, remembering how much of the pipe it has
	 * consumed (see #position).
	 */
	DisposableBuffer ReleaseBuffer() noexcept;

	FileDescriptor GetPipe() const noexcept {
		return event.GetFileDescriptor();
	}

	void DeferRead() noexcept {
		defer_read.Schedule();
	}

	void CancelRead() noexcept {
		event.CancelOnlyRead();
		defer_read.Cancel();
	}

	void TryRead();
	void OnPipeReady(unsigned events) noexcept;
	void OnDeferredRead() noexcept;
};

} // namespace Was
