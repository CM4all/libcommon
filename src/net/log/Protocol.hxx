/*
 * Copyright 2007-2018 Content Management AG
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

/*
 * Definitions for the beng-proxy logging protocol.
 */

#include <stdint.h>

namespace Net {
namespace Log {

/*

  Each log record is transmitted in a datagram (e.g. UDP).  All
  integers are in network byte order (big-endian).

  The datagram payload starts with a "magic" 32 bit number which
  identifies this datagram (#MAGIC_V2).

  After the magic, there are a variable number of attributes.  The
  first byte identifies the attribute according to enum Attribute,
  followed by a payload, which is specific to the attribute.  Strings
  are null-terminated.

  The attributes should be sorted by their identification bytes to
  allow older parsers to extract all attributes they know, and ignore
  newer ones at the end.

  The last four bytes of the datagram are a CRC32-CCITT of the
  payload, excluding the magic (and of course excluding the CRC
  itself).

 */

/**
 * Protocol version 1 magic number.
 */
static constexpr uint32_t MAGIC_V1 = 0x63046102;

/**
 * Protocol version 2 magic number.  Changes:
 *
 * - a CRC32-CCITT is at the end of the datagram
 */
static constexpr uint32_t MAGIC_V2 = 0x63046103;

enum class Attribute : uint8_t {
	NOP = 0,

	/**
	 * A 64 bit time stamp of the event, microseconds since epoch.
	 */
	TIMESTAMP = 1,

	/**
	 * The address of the remote host as a string.
	 */
	REMOTE_HOST = 2,

	/**
	 * The name of the site which was accessed.
	 */
	SITE = 3,

	/**
	 * The request method (http_method_t) as a 8 bit integer.
	 */
	HTTP_METHOD = 4,

	/**
	 * The request URI.
	 */
	HTTP_URI = 5,

	/**
	 * The "Referer"[sic] request header.
	 */
	HTTP_REFERER = 6,

	/**
	 * The "User-Agent" request header.
	 */
	USER_AGENT = 7,

	/**
	 * The response status (http_status_t) as a 16 bit integer.
	 */
	HTTP_STATUS = 8,

	/**
	 * The netto length of the entity in bytes, as a 64 bit integer.
	 */
	LENGTH = 9,

	/**
	 * The total number of raw bytes received and sent for this event,
	 * as two 64 bit integers.  This includes all extra data such as
	 * headers.
	 */
	TRAFFIC = 10,

	/**
	 * The wallclock duration of the operation as a 64 bit unsigned
	 * integer specifying the number of microseconds.
	 */
	DURATION = 11,

	/**
	 * The "Host" request header.
	 */
	HOST = 12,

	/**
	 * An opaque one-line message (without a trailing newline
	 * character).  This is used for error logging, not for HTTP
	 * access logging.
	 */
	MESSAGE = 13,

	/**
	 * The (string) address of the host which this request has been
	 * forwarded to.
	 */
	FORWARDED_TO = 14,

	/**
	 * The record type.  Payload is a #Type.
	 *
	 * Note that older clients do not emit this attribute, and
	 * parser have to guess it by checking which other attributes
	 * are present.
	 */
	TYPE = 15,
};

/**
 * The record type.  Payload of #Attrbute::TYPE.
 */
enum class Type : uint8_t {
	/**
	 * Unspecified.  The presence of HTTP-specific attributes can
	 * allow the parser to guess it's #HTTP_ACCESS or #HTTP_ERROR.
	 */
	UNSPECIFIED = 0,

	/**
	 * An HTTP access log record.  The record usually also
	 * contains #Attribute::HTTP_METHOD, #Attribute::HTTP_URI etc.
	 */
	HTTP_ACCESS = 1,

	/**
	 * An HTTP error log line.  The record usually also contains
	 * #Attribute::MESSAGE, and maybe attributes describing the
	 * HTTP request which caused the log event.
	 */
	HTTP_ERROR = 2,

	/**
	 * A mail submission log line.  The record usually also
	 * contains #Attribute::MESSAGE.
	 */
	SUBMISSON = 3,
};

}}
