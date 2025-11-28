// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Output.hxx"
#include "util/DisposableBuffer.hxx"

#include <exception> // for std::exception_ptr

class UniqueFileDescriptor;

namespace Was {

class SimpleOutputHandler {
public:
	virtual void OnWasOutputError(std::exception_ptr error) noexcept = 0;
};

class SimpleOutput final : OutputHandler, OutputProducer {
	Output output;

	SimpleOutputHandler &handler;

	DisposableBuffer buffer;

public:
	SimpleOutput(EventLoop &event_loop, UniqueFileDescriptor &&pipe,
		     SimpleOutputHandler &_handler) noexcept;
	~SimpleOutput() noexcept;

	auto &GetEventLoop() const noexcept {
		return output.GetEventLoop();
	}

	void Close() noexcept {
		output.Close();
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
		output.ResetPosition();
	}

	/**
	 * Handle a STOP command.  Returns the number of bytes already
	 * written to the pipe.  This may be called even after writing
	 * has completed (because the `position` field does not get
	 * cleared).
	 */
	std::size_t Stop() noexcept {
		buffer = {};
		return output.Stop();
	}

private:
	// virtual methods from class OutputHandler
	void OnWasOutputError(std::exception_ptr &&error) noexcept override;

	// virtual methods from class OutputProducer
	void OnWasOutputReady(FileDescriptor pipe) override;
};

} // namespace Was
