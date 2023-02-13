// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "was/async/CoRun.hxx"
#include "json/Was.hxx"
#include "json/ToDisposableBuffer.hxx"
#include "event/Loop.hxx"
#include "uri/MapQueryString.hxx"
#include "util/PrintException.hxx"

#include <boost/json.hpp>

static Co::Task<Was::SimpleResponse>
MyHandler(Was::SimpleRequest request)
{
	const auto j = Was::ParseJson(request);

	co_return Was::ToResponse(j);
}

int
main(int, char **) noexcept
try {
	EventLoop event_loop;

	Was::Run(event_loop, MyHandler);
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
