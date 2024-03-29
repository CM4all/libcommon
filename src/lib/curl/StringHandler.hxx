// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Handler.hxx"
#include "StringResponse.hxx"
#include "util/SpanCast.hxx"

/**
 * A #CurlResponseHandler implementation which stores the response
 * body in a std::string.
 */
class StringCurlResponseHandler : public CurlResponseHandler {
	StringCurlResponse response;

	std::exception_ptr error;

public:
	void CheckRethrowError() const {
		if (error)
			std::rethrow_exception(error);
	}

	StringCurlResponse GetResponse() && {
		CheckRethrowError();

		return std::move(response);
	}

public:
	/* virtual methods from class CurlResponseHandler */

	void OnHeaders(HttpStatus status, Curl::Headers &&headers) override {
		response.status = status;
		response.headers = std::move(headers);
	}

	void OnData(std::span<const std::byte> data) override {
		response.body.append(ToStringView(data));
	}

	void OnEnd() override {
	}

	void OnError(std::exception_ptr e) noexcept override {
		error = std::move(e);
	}
};
