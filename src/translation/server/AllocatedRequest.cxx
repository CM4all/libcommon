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

#include "AllocatedRequest.hxx"
#include "../Protocol.hxx"
#include "util/Compiler.h"
#include "util/ConstBuffer.hxx"
#include "util/RuntimeError.hxx"

#include <assert.h>

static std::string
ToString(ConstBuffer<void> b) noexcept
{
	auto c = ConstBuffer<char>::FromVoid(b);
	return {c.data, c.size};
}

namespace Translation::Server {

void
AllocatedRequest::Parse(TranslationCommand cmd, ConstBuffer<void> payload)
{
	switch (cmd) {
	case TranslationCommand::BEGIN:
		*this = {};

		if (payload.size >= 1)
			protocol_version = *(const uint8_t *)payload.data;

		break;

	case TranslationCommand::END:
		assert(false);
		gcc_unreachable();

	case TranslationCommand::URI:
		uri_buffer = ToString(payload);
		uri = uri_buffer.c_str();
		break;

	case TranslationCommand::HOST:
		host_buffer = ToString(payload);
		host = host_buffer.c_str();
		break;

	case TranslationCommand::ARGS:
		args_buffer = ToString(payload);
		args = args_buffer.c_str();
		break;

	case TranslationCommand::QUERY_STRING:
		query_string_buffer = ToString(payload);
		query_string = query_string_buffer.c_str();
		break;

	case TranslationCommand::USER_AGENT:
		user_agent_buffer = ToString(payload);
		user_agent = user_agent_buffer.c_str();
		break;

	case TranslationCommand::UA_CLASS:
		ua_class_buffer = ToString(payload);
		ua_class = ua_class_buffer.c_str();
		break;

	case TranslationCommand::LANGUAGE:
		accept_language_buffer = ToString(payload);
		accept_language = accept_language_buffer.c_str();
		break;

	case TranslationCommand::AUTHORIZATION:
		authorization_buffer = ToString(payload);
		authorization = authorization_buffer.c_str();
		break;

	case TranslationCommand::LISTENER_TAG:
		listener_tag_buffer = ToString(payload);
		listener_tag = listener_tag_buffer.c_str();
		break;

	case TranslationCommand::REMOTE_HOST:
	case TranslationCommand::LOCAL_ADDRESS:
	case TranslationCommand::LOCAL_ADDRESS_STRING:
		/* ignore */
		break;

	default:
		throw FormatRuntimeError("unknown translation packet: %u",
					 unsigned(cmd));
	}
}

} // namespace Translation::Server
