/*
 * Copyright 2020 CM4all GmbH
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

#include "StringRequestBody.hxx"
#include "ExceptionResponse.hxx"

#include <was/simple.h>

namespace Was {

std::string
RequestBodyToString(was_simple *w, std::string::size_type limit)
{
	using size_type = std::string::size_type;

	if (!was_simple_has_body(w))
		throw BadRequest{"Request body expected\n"};

	std::string s;

	while (true) {
		assert(s.length() <= limit);

		const auto remaining = was_simple_input_remaining(w);
		if (remaining == 0)
			break;

		if (remaining > 0) {
			if (uint64_t(remaining) > uint64_t(limit - s.length()))
				throw RequestBodyTooLarge{};

			s.reserve(s.length() + size_type(remaining));
		}

		char buffer[16384];
		const ssize_t r = was_simple_read(w, buffer, sizeof(buffer));
		if (r < 0)
			throw AbortResponse{};

		if (r == 0)
			break;

		const size_type nbytes = r;
		if (nbytes > limit - s.length())
			throw RequestBodyTooLarge{};

		s.append(buffer, nbytes);
	}

	return s;
}

} // namespace Was
