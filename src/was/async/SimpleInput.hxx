// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "event/DeferEvent.hxx"

#include <memory>

class UniqueFileDescriptor;
class DisposableBuffer;

namespace Was {

class Buffer;

class SimpleInputHandler {
public:
	virtual void OnWasInput(DisposableBuffer input) noexcept = 0;
	virtual void OnWasInputError(std::exception_ptr error) noexcept = 0;
};

class SimpleInput final {
	PipeEvent event;
	DeferEvent defer_read;

	SimpleInputHandler &handler;

	std::unique_ptr<Buffer> buffer;

public:
	SimpleInput(EventLoop &event_loop, UniqueFileDescriptor pipe,
		    SimpleInputHandler &_handler) noexcept;
	~SimpleInput() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	bool IsActive() const noexcept {
		return buffer != nullptr;
	}

	void Activate() noexcept;

	bool SetLength(std::size_t length) noexcept;

	DisposableBuffer CheckComplete() noexcept;

	/**
	 * Throws on error.
	 */
	void Premature(std::size_t nbytes);

private:
	FileDescriptor GetPipe() const noexcept {
		return event.GetFileDescriptor();
	}

	void DeferRead() noexcept {
		defer_read.Schedule();
	}

	void TryRead();
	void OnPipeReady(unsigned events) noexcept;
	void OnDeferredRead() noexcept;
};

} // namespace Was
