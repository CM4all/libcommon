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

	/**
	 * Set a "text/plain" body referring to the specified string.
	 * It must remain valid until the response has been written to
	 * the pipe.
	 */
	void SetTextPlain(std::string_view _body) noexcept;

	/**
	 * Like SetTextPlain(), but move from a std::string.
	 */
	void MoveTextPlain(std::string &&_body) noexcept;

	/**
	 * Like SetTextPlain(), but copy the specified string.
	 */
	void CopyTextPlain(std::string_view _body) noexcept;

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
