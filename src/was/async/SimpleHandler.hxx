/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "http/Method.h"
#include "http/Status.h"
#include "util/DisposableBuffer.hxx"

#include <map>
#include <string>

class CancellablePointer;

namespace Was {

struct SimpleRequest {
	std::string remote_host;
	std::map<std::string, std::string, std::less<>> parameters;
	http_method_t method;
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
	http_status_t status = HTTP_STATUS_OK;
	std::multimap<std::string, std::string, std::less<>> headers;
	DisposableBuffer body;

	void SetTextPlain(std::string_view _body) noexcept {
		body = {ToNopPointer(_body.data()), _body.size()};
		headers.emplace("content-type", "text/plain");
	}

	static SimpleResponse MethodNotAllowed(std::string allow) noexcept {
		return {
			HTTP_STATUS_METHOD_NOT_ALLOWED,
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
