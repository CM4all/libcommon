// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Splice.hxx"
#include "io/FileDescriptor.hxx"

extern "C" {
#include <was/simple.h>
}

#include <cerrno>
#include <limits>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

bool
SpliceToWas(was_simple *w, FileDescriptor in_fd, uint64_t remaining) noexcept
{
	if (remaining == 0)
		return true;

	const FileDescriptor out_fd(was_simple_output_fd(w));
	while (remaining > 0) {
		const auto poll_result = was_simple_output_poll(w, -1);
		switch (poll_result) {
		case WAS_SIMPLE_POLL_SUCCESS:
			break;

		case WAS_SIMPLE_POLL_ERROR:
			fprintf(stderr, "Error sending HTTP response body.\n");
			return false;

		case WAS_SIMPLE_POLL_TIMEOUT:
			fprintf(stderr, "Timeout writing HTTP response body.\n");
			return false;

		case WAS_SIMPLE_POLL_END:
			/* how can this happen if "remaining>0"? */
			return true;

		case WAS_SIMPLE_POLL_CLOSED:
			fprintf(stderr, "Client has closed the GET response body.\n");
			return false;
		}

		constexpr uint64_t max = std::numeric_limits<size_t>::max();
		size_t length = std::min(remaining, max);
		ssize_t nbytes = splice(in_fd.Get(), nullptr,
					out_fd.Get(), nullptr,
					length,
					SPLICE_F_MOVE);
		if (nbytes <= 0) {
			if (nbytes < 0)
				fprintf(stderr, "splice() failed: %s\n", strerror(errno));
			return false;
		}

		if (!was_simple_sent(w, nbytes))
			return false;

		remaining -= nbytes;
	}

	return true;
}
