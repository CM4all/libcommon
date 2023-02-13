// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
