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

class SimpleOutput final : public OutputProducer {
	Output &output;

	SimpleOutputHandler &handler;

	const DisposableBuffer buffer;

public:
	SimpleOutput(Output &_output, DisposableBuffer &&_buffer,
		     SimpleOutputHandler &_handler) noexcept;
	~SimpleOutput() noexcept;

private:
	// virtual methods from class OutputProducer
	void OnWasOutputReady() override;
};

} // namespace Was
