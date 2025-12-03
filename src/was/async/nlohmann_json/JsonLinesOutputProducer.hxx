// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "was/async/Producer.hxx"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <list>

namespace Was {

class Output;

/**
 * An #OutputProducer implementation which provides a never-ending
 * stream of JSON lines.  New lines are added with Push().
 */
class JsonLinesOutputProducer : public OutputProducer {
	Output *output = nullptr;

	struct Line;
	std::list<Line> lines;

	std::size_t column = 0;

	std::size_t size = 0;

	const std::size_t limit;

public:
	/**
	 * @param _limit if non-zero, then all lines will be discarded
	 * while the buffer has at least this number of bytes
	 */
	explicit JsonLinesOutputProducer(std::size_t _limit=0) noexcept;
	~JsonLinesOutputProducer() noexcept override;

	bool IsFull() const noexcept {
		return size >= limit;
	}

	/**
	 * @return true on success, false if the buffer is full
	 */
	bool Push(const nlohmann::json &j) noexcept;

private:
	// virtual methods from class OutputProducer
	bool OnWasOutputBegin(Output &_output) noexcept override;
	void OnWasOutputReady() override;
};

} // namespace Was
