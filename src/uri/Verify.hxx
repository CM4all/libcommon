/*
 * Copyright 2007-2021 CM4all GmbH
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

/*
 * Verify URI parts.
 */

#pragma once

#include <string_view>

/**
 * Is this a valid domain name (i.e. host name) according to RFC 1034
 * 3.5?
 */
[[gnu::pure]]
bool
VerifyDomainName(std::string_view name) noexcept;

/**
 * Is this a valid "host:port" string (or "Host:" request header)
 * according to RFC 2616 3.2.2 / 14.23?
 */
[[gnu::pure]]
bool
VerifyUriHostPort(std::string_view host_port) noexcept;

/**
 * Verifies one path segment of an URI according to RFC 2396.
 */
[[gnu::pure]]
bool
uri_segment_verify(std::string_view segment) noexcept;

/**
 * Verifies the path portion of an URI according to RFC 2396.
 */
[[gnu::pure]]
bool
uri_path_verify(std::string_view uri) noexcept;

/**
 * Performs some paranoid checks on the URI; the following is not
 * allowed:
 *
 * - %00
 * - %2f (encoded slash)
 * - "/../", "/./"
 * - "/..", "/." at the end
 *
 * It is assumed that the URI was already verified with
 * uri_path_verify().
 */
[[gnu::pure]]
bool
uri_path_verify_paranoid(const char *uri) noexcept;

/**
 * Quickly verify the validity of an URI (path plus query).  This may
 * be used before passing it to another server, not to be parsed by
 * this process.
 */
[[gnu::pure]]
bool
uri_path_verify_quick(const char *uri) noexcept;
