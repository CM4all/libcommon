// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "event/DeferEvent.hxx"

#include <cassert>
#include <memory>
#include <exception> // for std::exception_ptr

class UniqueFileDescriptor;

namespace Was {

class OutputProducer;

class OutputHandler {
public:
	virtual void OnWasOutputError(std::exception_ptr &&error) noexcept = 0;
};

/**
 * Generic class for a non-blocking WAS output.
 */
class Output final {
	PipeEvent event;
	DeferEvent defer_write;

	OutputHandler *handler;

	std::unique_ptr<OutputProducer> producer;

	std::size_t position;

public:
	Output(EventLoop &event_loop, UniqueFileDescriptor &&pipe,
	       OutputHandler &_handler) noexcept;
	~Output() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Close() noexcept {
		event.Close();
		defer_write.Cancel();
	}

	/**
	 * Install a different handler.
	 */
	void SetHandler(OutputHandler &_handler) noexcept {
		handler = &_handler;
	}

	bool IsActive() const noexcept {
		return producer != nullptr;
	}

	void Activate(std::unique_ptr<OutputProducer> &&_producer) noexcept;
	void Deactivate() noexcept;

	/**
	 * Provides access to the underlying pipe.  The producer may
	 * write to it; after a successful write, call AddPosition().
	 */
	FileDescriptor GetPipe() noexcept {
		return event.GetFileDescriptor();
	}

	/**
	 * If an OutputProducer::OnWasOutputReady() call is pending
	 * because the pipe was determined to be ready for writing,
	 * cancel that call for this #EventLoop iteration.  The
	 * producer may call this after a successful write to avoid
	 * writing twice per #EventLoop iteration.
	 */
	void ClearReadyFlag() noexcept {
		event.ClearReadyFlags(PipeEvent::WRITE);
	}

	std::size_t GetPosition() const noexcept {
		return position;
	}

	void AddPosition(std::size_t nbytes) noexcept {
		assert(IsActive());

		position += nbytes;
	}

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
		Deactivate();
		return position;
	}

	void ScheduleWrite() noexcept {
		event.ScheduleWrite();
	}

	void DeferWrite() noexcept {
		defer_write.Schedule();
	}

	void DeferNextWrite() noexcept {
		defer_write.ScheduleNext();
	}

	void CancelWrite() noexcept {
		event.ScheduleImplicit();
		defer_write.Cancel();
	}

private:
	void TryWrite();
	void OnDeferredWrite() noexcept;
	void OnPipeReady(unsigned events) noexcept;
};

} // namespace Was
