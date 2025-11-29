// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "was/async/SimpleRun.hxx"
#include "was/async/SimpleOutput.hxx"
#include "was/async/SimpleResponse.hxx"
#include "was/async/SimpleServer.hxx"
#include "event/Loop.hxx"
#include "util/PrintException.hxx"
#include "DefaultFifoBuffer.hxx"

class MyHandler final : public Was::SimpleRequestHandler {
public:
	bool OnRequest(Was::SimpleServer &server,
			  Was::SimpleRequest &&request,
			  CancellablePointer &) noexcept override {
		return server.SendResponse({
				HttpStatus::OK,
				std::move(request.headers),
				request.body ? std::make_unique<Was::SimpleOutput>(std::move(request.body)) : nullptr,
			});
	}
};

int
main(int, char **) noexcept
try {
	[[maybe_unused]]
	const ScopeInitDefaultFifoBuffer init_default_fifo_buffer;

	EventLoop event_loop;
	MyHandler h;

	Was::Run(event_loop, h);
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
