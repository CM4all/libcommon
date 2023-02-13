// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "http/Method.hxx"
#include "http/Status.hxx"
#include "util/DisposableBuffer.hxx"

#include <map>
#include <string>

class CancellablePointer;

namespace Was {

struct SimpleRequest {
	std::string remote_host;
	std::map<std::string, std::string, std::less<>> parameters;
	HttpMethod method;
	std::string uri;
	std::string script_name, path_info, query_string;
	std::multimap<std::string, std::string, std::less<>> headers;
	DisposableBuffer body;

	/**
	 * Compare the base of the Content-Type header with the given
	 * expected value.
	 */
	[[gnu::pure]]
	bool IsContentType(const std::string_view expected) const noexcept;
};

struct SimpleResponse {
	HttpStatus status = HttpStatus::OK;
	std::multimap<std::string, std::string, std::less<>> headers;
	DisposableBuffer body;

	void SetTextPlain(std::string_view _body) noexcept {
		body = {ToNopPointer(_body.data()), _body.size()};
		headers.emplace("content-type", "text/plain");
	}

	static SimpleResponse MethodNotAllowed(std::string allow) noexcept {
		return {
			HttpStatus::METHOD_NOT_ALLOWED,
			std::multimap<std::string, std::string, std::less<>>{
				{"allow", std::move(allow)},
			},
			nullptr,
		};
	}
};

class SimpleServer;

class SimpleRequestHandler {
public:
	/**
	 * A request was received.  The implementation shall handle it
	 * and call SimpleServer::SendResponse().
	 *
	 * @return false if the #SimpleServer was closed
	 */
	virtual bool OnRequest(SimpleServer &server,
			       SimpleRequest &&request,
			       CancellablePointer &cancel_ptr) noexcept = 0;
};

} // namespace Was
