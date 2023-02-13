// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Request.hxx"
#include "Handler.hxx"
#include "co/Compat.hxx"

#include <exception>

namespace Curl {

struct CoResponse {
	HttpStatus status = {};

	Headers headers;

	std::string body;
};

/**
 * A CURL HTTP request as a C++20 coroutine.
 */
class CoRequest final : CurlResponseHandler {
	CurlRequest request;

	CoResponse response;
	std::exception_ptr error;

	std::coroutine_handle<> continuation;

	bool ready = false;

public:
	CoRequest(CurlGlobal &global, CurlEasy easy);

	auto operator co_await() noexcept {
		struct Awaitable final {
			CoRequest &request;

			bool await_ready() const noexcept {
				return request.ready;
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) const noexcept {
				request.continuation = _continuation;
				return std::noop_coroutine();
			}

			CoResponse await_resume() const {
				if (request.error)
					std::rethrow_exception(std::move(request.error));

				return std::move(request.response);
			}
		};

		return Awaitable{*this};
	}

private:
	/* virtual methods from CurlResponseHandler */
	void OnHeaders(HttpStatus status, Headers &&headers) override;
	void OnData(std::span<const std::byte> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};

} // namespace Curl
