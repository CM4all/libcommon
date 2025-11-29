// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "http/Status.hxx"
#include "util/DisposableBuffer.hxx"

#include <map>
#include <string>

class CancellablePointer;

namespace Was {

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

} // namespace Was
