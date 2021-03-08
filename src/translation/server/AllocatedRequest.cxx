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

	case TranslationCommand::SESSION:
		session_buffer = ToString(payload);
		session = {session_buffer.data(), session_buffer.size()};
		break;

	case TranslationCommand::PARAM:
		param_buffer = ToString(payload);
		param = param_buffer.c_str();
		break;

	case TranslationCommand::USER:
		user_buffer = ToString(payload);
		user = user_buffer.c_str();
		break;

	case TranslationCommand::PASSWORD:
		password_buffer = ToString(payload);
		password = password_buffer.c_str();
		break;

	case TranslationCommand::STATUS:
		if (payload.size != 2)
			throw std::runtime_error("size mismatch in STATUS packet");

		status = http_status_t(*(const uint16_t*)payload.data);
		/* TODO enable
		if (!http_status_is_valid(status))
			throw FormatRuntimeError("invalid HTTP status code %u",
						 status);
		*/

		break;

	case TranslationCommand::WIDGET_TYPE:
		widget_type_buffer = ToString(payload);
		widget_type = widget_type_buffer.c_str();
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

	case TranslationCommand::LANGUAGE:
		accept_language_buffer = ToString(payload);
		accept_language = accept_language_buffer.c_str();
		break;

	case TranslationCommand::AUTHORIZATION:
		authorization_buffer = ToString(payload);
		authorization = authorization_buffer.c_str();
		break;

	case TranslationCommand::ERROR_DOCUMENT:
		error_document_buffer = ToString(payload);
		error_document = {error_document_buffer.data(), error_document_buffer.size()};
		break;

	case TranslationCommand::HTTP_AUTH:
		http_auth_buffer = ToString(payload);
		http_auth = {http_auth_buffer.data(), http_auth_buffer.size()};
		break;

	case TranslationCommand::TOKEN_AUTH:
		token_auth_buffer = ToString(payload);
		token_auth = {token_auth_buffer.data(), token_auth_buffer.size()};
		break;

	case TranslationCommand::AUTH_TOKEN:
		auth_token_buffer = ToString(payload);
		auth_token = auth_token_buffer.c_str();
		break;

	case TranslationCommand::RECOVER_SESSION:
		recover_session_buffer = ToString(payload);
		recover_session = recover_session_buffer.c_str();
		break;

	case TranslationCommand::CHECK:
		check_buffer = ToString(payload);
		check = {check_buffer.data(), check_buffer.size()};
		break;

	case TranslationCommand::WANT_FULL_URI:
		want_full_uri_buffer = ToString(payload);
		want_full_uri = {want_full_uri_buffer.data(), want_full_uri_buffer.size()};
		break;

	case TranslationCommand::FILE_NOT_FOUND:
		file_not_found_buffer = ToString(payload);
		file_not_found = {file_not_found_buffer.data(), file_not_found_buffer.size()};
		break;

	case TranslationCommand::CONTENT_TYPE_LOOKUP:
		content_type_lookup_buffer = ToString(payload);
		content_type_lookup = {content_type_lookup_buffer.data(), content_type_lookup_buffer.size()};
		break;

	case TranslationCommand::SUFFIX:
		suffix_buffer = ToString(payload);
		suffix = suffix_buffer.c_str();
		break;

	case TranslationCommand::DIRECTORY_INDEX:
		directory_index_buffer = ToString(payload);
		directory_index = {directory_index_buffer.data(), directory_index_buffer.size()};
		break;

	case TranslationCommand::ENOTDIR_:
		enotdir_buffer = ToString(payload);
		enotdir = {enotdir_buffer.data(), enotdir_buffer.size()};
		break;

	case TranslationCommand::AUTH:
		auth_buffer = ToString(payload);
		auth = {auth_buffer.data(), auth_buffer.size()};
		break;

	case TranslationCommand::PROBE_PATH_SUFFIXES:
		probe_path_suffixes_buffer = ToString(payload);
		probe_path_suffixes = {probe_path_suffixes_buffer.data(), probe_path_suffixes_buffer.size()};
		break;

	case TranslationCommand::PROBE_SUFFIX:
		probe_suffix_buffer = ToString(payload);
		probe_suffix = probe_suffix_buffer.c_str();
		break;

	case TranslationCommand::LISTENER_TAG:
		listener_tag_buffer = ToString(payload);
		listener_tag = listener_tag_buffer.c_str();
		break;

	case TranslationCommand::READ_FILE:
		read_file_buffer = ToString(payload);
		read_file = {read_file_buffer.data(), read_file_buffer.size()};
		break;

	case TranslationCommand::INTERNAL_REDIRECT:
		internal_redirect_buffer = ToString(payload);
		internal_redirect = {internal_redirect_buffer.data(), internal_redirect_buffer.size()};
		break;

	case TranslationCommand::LOGIN:
		login = true;
		break;

	case TranslationCommand::CRON:
		cron = true;
		break;

	case TranslationCommand::POOL:
		pool_buffer = ToString(payload);
		pool = pool_buffer.c_str();
		break;

	case TranslationCommand::SERVICE:
		service_buffer = ToString(payload);
		service = service_buffer.c_str();
		break;

	case TranslationCommand::CHAIN:
		chain_buffer = ToString(payload);
		chain = {chain_buffer.data(), chain_buffer.size()};
		break;

	case TranslationCommand::CHAIN_HEADER:
		chain_header_buffer = ToString(payload);
		chain_header = chain_header_buffer.c_str();
		break;

	case TranslationCommand::REMOTE_HOST:
	case TranslationCommand::LOCAL_ADDRESS:
	case TranslationCommand::LOCAL_ADDRESS_STRING:
		/* ignore */
		break;

	case TranslationCommand::LAYOUT:
		layout_buffer = ToString(payload);
		layout = {layout_buffer.data(), layout_buffer.size()};
		break;

	case TranslationCommand::BASE:
		base_buffer = ToString(payload);
		base = base_buffer.c_str();
		break;

	case TranslationCommand::REGEX:
		regex_buffer = ToString(payload);
		regex = regex_buffer.c_str();
		break;

	default:
		throw FormatRuntimeError("unknown translation packet: %u",
					 unsigned(cmd));
	}
}

} // namespace Translation::Server
