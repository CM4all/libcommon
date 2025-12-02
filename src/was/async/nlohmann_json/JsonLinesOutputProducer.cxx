// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "JsonLinesOutputProducer.hxx"
#include "was/async/Output.hxx"
#include "util/SpanCast.hxx"

#include <nlohmann/json.hpp>

#include <cassert>

using std::string_view_literals::operator""sv;

namespace Was {

struct JsonLinesOutputProducer::Line {
	std::string value;
};

JsonLinesOutputProducer::JsonLinesOutputProducer(std::size_t _limit) noexcept
	:limit(_limit) {}

JsonLinesOutputProducer::~JsonLinesOutputProducer() noexcept = default;

bool
JsonLinesOutputProducer::Push(const nlohmann::json &j) noexcept
{
	if (IsFull())
		return false;

	lines.emplace_back(j.dump());
	size += lines.back().value.size();

	output->DeferWrite();
	return true;
}

bool
JsonLinesOutputProducer::OnWasOutputBegin(Output &_output) noexcept
{
	output = &_output;
	return true;
}

void
JsonLinesOutputProducer::OnWasOutputReady()
{
	// TODO use writev()

	while (!lines.empty()) {
		const auto r = AsBytes(lines.front().value).subspan(column);
		if (r.empty()) {
			const std::size_t nbytes = output->Write(AsBytes("\n"sv));
			if (nbytes == 0)
				return;

			size -= lines.front().value.size();
			lines.pop_front();
			column = 0;
		} else {
			const std::size_t nbytes = output->Write(r);
			column += nbytes;
			if (nbytes < r.size())
				return;
		}
	}

	output->CancelWrite();
}

} // namespace Was
