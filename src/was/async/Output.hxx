// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "event/DeferEvent.hxx"

#include <cassert>
#include <memory>
#include <exception> // for std::exception_ptr

#ifndef NDEBUG
#include <optional>
#endif

class UniqueFileDescriptor;

namespace Was {

class OutputProducer;

class OutputHandler {
public:
	virtual bool OnWasOutputLength(uint_least64_t length) noexcept = 0;
	virtual void OnWasOutputEnd() noexcept = 0;
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

	uint_least64_t position;

#ifndef NDEBUG
	std::optional<uint_least64_t> length;
#endif

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

	/**
	 * @return false if this object has been destroyed
	 */
	[[nodiscard]]
	bool Activate(std::unique_ptr<OutputProducer> &&_producer) noexcept;

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

	uint_least64_t GetPosition() const noexcept {
		return position;
	}

	void AddPosition(uint_least64_t nbytes) noexcept {
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
	uint_least64_t Stop() noexcept {
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

	/**
	 * Called by the #OutputProducer once the stream length is
	 * known.  Must be called after OnWasOutputBegin() is called
	 * (or from inside that method) but before End() is called.
	 *
	 * @return false if the #OutputProducer has been destroyed
	 */
	[[nodiscard]]
	bool SetLength(uint_least64_t _length) noexcept {
#ifndef NDEBUG
		assert(!length);
		length = _length;
#endif

		return handler->OnWasOutputLength(_length);
	}

	/**
	 * Called by the #OutputProducer once the stream is finished.
	 * After returning, the #OutputProducer has been deleted.
	 */
	void End() noexcept {
		assert(length);
		assert(position == *length);

		handler->OnWasOutputEnd();
	}

private:
	void TryWrite();
	void OnDeferredWrite() noexcept;
	void OnPipeReady(unsigned events) noexcept;
};

} // namespace Was
