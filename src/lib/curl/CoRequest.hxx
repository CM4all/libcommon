// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Request.hxx"
#include "Handler.hxx"
#include "co/AwaitableHelper.hxx"

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

	using Awaitable = Co::AwaitableHelper<CoRequest>;
	friend Awaitable;

public:
	CoRequest(CurlGlobal &global, CurlEasy easy);

	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return ready;
	}

	CoResponse TakeValue() noexcept {
		return std::move(response);
	}

	/* virtual methods from CurlResponseHandler */
	void OnHeaders(HttpStatus status, Headers &&headers) override;
	void OnData(std::span<const std::byte> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};

} // namespace Curl
