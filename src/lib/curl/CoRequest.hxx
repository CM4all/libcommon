// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Request.hxx"
#include "Handler.hxx"
#include "StringOptions.hxx"
#include "StringResponse.hxx"
#include "co/AwaitableHelper.hxx"

#include <exception>

namespace Curl {

/**
 * A CURL HTTP request as a C++20 coroutine.
 */
class CoRequest final : CurlResponseHandler {
	CurlRequest request;

	StringResponse response;
	std::exception_ptr error;

	std::coroutine_handle<> continuation;

	const StringOptions options;

	bool ready = false;

	using Awaitable = Co::AwaitableHelper<CoRequest>;
	friend Awaitable;

public:
	CoRequest(CurlGlobal &global, CurlEasy easy, StringOptions _options={});

	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return ready;
	}

	StringResponse TakeValue() noexcept {
		return std::move(response);
	}

	/* virtual methods from CurlResponseHandler */
	void OnHeaders(HttpStatus status, Headers &&headers) override;
	void OnData(std::span<const std::byte> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};

} // namespace Curl
