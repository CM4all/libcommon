// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>

class FileDescriptor;

namespace Net::Log {

struct Datagram;

struct OneLineOptions {
	/**
	 * Show the time stamp in ISO8601 format?
	 */
	bool iso8601 = false;

	/**
	 * Show the site name?
	 */
	bool show_site = false;

	/**
	 * Show the HTTP "Host" header?
	 */
	bool show_host = false;

	/**
	 * anonymize IP addresses by zeroing a portion at the end?
	 */
	bool anonymize = false;

	/**
	 * Show the address of the server this request was forwarded
	 * to?
	 */
	bool show_forwarded_to = false;

	/**
	 * Show the HTTP "Referer" header?
	 */
	bool show_http_referer = true;

	/**
	 * Show the HTTP "User-Agent" header?
	 */
	bool show_user_agent = true;
};

/**
 * Convert the given datagram to a text line (without a trailing
 * newline character and without a null terminator).
 *
 * @return a pointer to the end of the line
 */
char *
FormatOneLine(char *buffer, std::size_t buffer_size,
	      const Datagram &d, OneLineOptions options) noexcept;

/**
 * Print the #Datagram in one line, similar to Apache's "combined" log
 * format.
 *
 * @return true on success, false on error (errno set)
 */
bool
LogOneLine(FileDescriptor fd, const Datagram &d,
	   OneLineOptions options) noexcept;

} // namespace Net::Log
