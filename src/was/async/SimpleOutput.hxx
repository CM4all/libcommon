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
	virtual void OnWasOutputEnd() noexcept = 0;
};

class SimpleOutput final : OutputProducer {
	Output &output;

	SimpleOutputHandler &handler;

	DisposableBuffer buffer;

public:
	SimpleOutput(Output &_output, SimpleOutputHandler &_handler) noexcept;
	~SimpleOutput() noexcept;

	void Activate(DisposableBuffer _buffer) noexcept;

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
	// virtual methods from class OutputProducer
	void OnWasOutputReady() override;
};

} // namespace Was
