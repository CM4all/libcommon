// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Producer.hxx"
#include "util/DisposableBuffer.hxx"

namespace Was {

class Output;

class SimpleOutput final : public OutputProducer {
	Output *output;

	const DisposableBuffer buffer;

public:
	[[nodiscard]]
	explicit SimpleOutput(DisposableBuffer &&_buffer) noexcept
		:buffer(std::move(_buffer)) {}

	~SimpleOutput() noexcept override = default;

private:
	// virtual methods from class OutputProducer
	bool OnWasOutputBegin(Output &_output) noexcept override;
	void OnWasOutputReady() override;
};

} // namespace Was
