// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "was/async/CoRun.hxx"
#include "was/ExceptionResponse.hxx"
#include "event/Loop.hxx"
#include "uri/MapQueryString.hxx"
#include "util/PrintException.hxx"
#include "DefaultFifoBuffer.hxx"

static Co::Task<Was::SimpleResponse>
MyHandler(Was::SimpleRequest)
{
	throw Was::BadRequest{};
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
