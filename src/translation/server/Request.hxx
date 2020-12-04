/*
 * Copyright 2007-2019 Content Management AG
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

#include "http/Status.h"
#include "util/ConstBuffer.hxx"

namespace Translation::Server {

struct Request {
	unsigned protocol_version = 0;

	http_status_t status = http_status_t(0);

	const char *uri = nullptr;

	const char *host = nullptr;

	ConstBuffer<void> session = nullptr;

	const char *param = nullptr;

	const char *user = nullptr;
	const char *password = nullptr;

	const char *args = nullptr, *query_string = nullptr;

	const char *widget_type = nullptr;

	const char *user_agent = nullptr;
	const char *ua_class = nullptr;
	const char *accept_language = nullptr;

	/**
	 * The value of the "Authorization" HTTP request header.
	 */
	const char *authorization = nullptr;

	ConstBuffer<void> error_document = nullptr;

	ConstBuffer<void> http_auth = nullptr;

	ConstBuffer<void> token_auth = nullptr;
	const char *auth_token = nullptr;

	ConstBuffer<void> check = nullptr;

	ConstBuffer<void> want_full_uri = nullptr;

	ConstBuffer<void> chain = nullptr;

	const char *chain_header = nullptr;

	ConstBuffer<void> file_not_found = nullptr;

	ConstBuffer<void> content_type_lookup = nullptr;

	const char *suffix = nullptr;

	ConstBuffer<void> directory_index = nullptr;

	ConstBuffer<void> enotdir = nullptr;

	ConstBuffer<void> auth = nullptr;

	ConstBuffer<void> probe_path_suffixes = nullptr;
	const char *probe_suffix = nullptr;

	const char *listener_tag = nullptr;

	ConstBuffer<void> read_file = nullptr;
	ConstBuffer<void> internal_redirect = nullptr;

	const char *pool = nullptr;

	const char *service = nullptr;

	bool login = false;

	bool cron = false;
};

} // namespace Translation::Server
