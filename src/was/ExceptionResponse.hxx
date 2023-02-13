// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

namespace Was {

/**
 * The base class for all exceptions in this header.
 *
 * These will be handled by WasLoop() and therefore should usually not
 * be handled by the request handler, but rethrown to WasLoop()
 * instead.
 */
struct Exception {};

/**
 * Indicates that was_simple_end() shall be called.  This can be used
 * to bail out of a handler function after a response has been
 * submitted.
 */
struct EndResponse : Exception {};

/**
 * Indicates that was_simple_abort() shall be called.  This can also
 * be used after an I/O or protocol error to bail out.
 */
struct AbortResponse : Exception {};

struct NotFound : Exception {
	std::string_view body = "Not Found\n";

	NotFound() noexcept = default;
	constexpr NotFound(std::string_view _body) noexcept
		:body(_body) {}
};

struct BadRequest : Exception {
	std::string_view body = "Bad Request\n";

	BadRequest() noexcept = default;
	constexpr BadRequest(std::string_view _body) noexcept
		:body(_body) {}
};

} // namespace Was
