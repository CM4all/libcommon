// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Producer.hxx"

#include <string>

namespace Was {

class Output;

/**
 * An #OutputProducer implementation that provides data from a std::string.
 */
class StringOutputProducer final : public OutputProducer {
	Output *output;

	std::string buffer;

public:
	[[nodiscard]]
	explicit StringOutputProducer(std::string &&_buffer) noexcept
		:buffer(std::move(_buffer)) {}

private:
	// virtual methods from class OutputProducer
	bool OnWasOutputBegin(Output &_output) noexcept override;
	void OnWasOutputReady() override;
};

} // namespace Was
