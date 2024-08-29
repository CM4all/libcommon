// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "was/async/CoRun.hxx"
#include "was/async/nlohmann_json/Read.hxx"
#include "was/async/nlohmann_json/Write.hxx"
#include "lib/nlohmann_json/ToDisposableBuffer.hxx"
#include "event/Loop.hxx"
#include "uri/MapQueryString.hxx"
#include "util/PrintException.hxx"
#include "DefaultFifoBuffer.hxx"

#include <nlohmann/json.hpp>

static Co::Task<Was::SimpleResponse>
MyHandler(Was::SimpleRequest request)
{
	const auto j = Was::ParseJson(request);

	co_return Was::ToResponse(j);
}

int
main(int, char **) noexcept
try {
	[[maybe_unused]]
	const ScopeInitDefaultFifoBuffer init_default_fifo_buffer;

	EventLoop event_loop;

	Was::Run(event_loop, MyHandler);
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
