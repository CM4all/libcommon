// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Producer.hxx"

#include <cstddef>
#include <span>

namespace Was {

class Output;

/**
 * An #OutputProducer implementation that provides data from a
 * std::span.  The pointed-to buffer is not owned by this class and
 * must remain valid as long as this object exists.
 */
class SpanOutputProducer final : public OutputProducer {
	Output *output;

	const std::span<const std::byte> buffer;

public:
	[[nodiscard]]
	explicit SpanOutputProducer(const std::span<const std::byte> _buffer) noexcept
		:buffer(_buffer) {}

private:
	// virtual methods from class OutputProducer
	bool OnWasOutputBegin(Output &_output) noexcept override;
	void OnWasOutputReady() override;
};

} // namespace Was
