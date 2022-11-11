/*
 * Copyright 2019-2022 CM4all GmbH
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
