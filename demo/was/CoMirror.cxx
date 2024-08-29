// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "was/async/CoRun.hxx"
#include "event/Loop.hxx"
#include "util/PrintException.hxx"
#include "DefaultFifoBuffer.hxx"

static Co::Task<Was::SimpleResponse>
MyHandler(Was::SimpleRequest request)
{
	co_return Was::SimpleResponse{
		HttpStatus::OK,
		std::move(request.headers),
		std::move(request.body),
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
