// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Producer.hxx"
#include "http/Status.hxx"

#include <map>
#include <memory>
#include <string>

class CancellablePointer;

namespace Was {

class OutputProducer;

struct SimpleResponse {
	HttpStatus status = HttpStatus::OK;
	std::multimap<std::string, std::string, std::less<>> headers;
	std::unique_ptr<OutputProducer> body;

	void SetTextPlain(std::string_view _body) noexcept;

	static SimpleResponse MethodNotAllowed(std::string allow) noexcept {
		return {
			HttpStatus::METHOD_NOT_ALLOWED,
			std::multimap<std::string, std::string, std::less<>>{
				{"allow", std::move(allow)},
			},
		};
	}
};

} // namespace Was
