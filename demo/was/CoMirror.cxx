// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "was/async/CoRun.hxx"
#include "was/async/SimpleOutput.hxx"
#include "was/async/SimpleResponse.hxx"
#include "event/Loop.hxx"
#include "util/PrintException.hxx"
#include "DefaultFifoBuffer.hxx"

static Co::Task<Was::SimpleResponse>
MyHandler(Was::SimpleRequest request)
{
	co_return Was::SimpleResponse{
		HttpStatus::OK,
		std::move(request.headers),
		request.body ? std::make_unique<Was::SimpleOutput>(std::move(request.body)) : nullptr,
	};
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
