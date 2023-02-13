// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AllocatedRequest.hxx"
#include "../Protocol.hxx"
#include "util/Compiler.h"
#include "util/RuntimeError.hxx"
#include "util/SpanCast.hxx"

#include <assert.h>

static std::string
ToString(std::span<const std::byte> b) noexcept
{
	return std::string{ToStringView(b)};
}

namespace Translation::Server {

void
AllocatedRequest::Parse(TranslationCommand cmd, std::span<const std::byte> payload)
{
	switch (cmd) {
	case TranslationCommand::BEGIN:
		*this = {};

		if (!payload.empty())
			protocol_version = static_cast<uint8_t>(payload.front());

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
		session = AsBytes(session_buffer);
		break;

	case TranslationCommand::REALM_SESSION:
		realm_session_buffer = ToString(payload);
		realm_session = AsBytes(realm_session_buffer);
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
		if (payload.size() != 2)
			throw std::runtime_error("size mismatch in STATUS packet");

		static_assert(sizeof(HttpStatus) == 2);
		status = *(const HttpStatus *)(const void *)payload.data();
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
		error_document = AsBytes(error_document_buffer);
		break;

	case TranslationCommand::HTTP_AUTH:
		http_auth_buffer = ToString(payload);
		http_auth = AsBytes(http_auth_buffer.data());
		break;

	case TranslationCommand::TOKEN_AUTH:
		token_auth_buffer = ToString(payload);
		token_auth = AsBytes(token_auth_buffer);
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
		check = AsBytes(check_buffer);
		break;

	case TranslationCommand::CHECK_HEADER:
		check_header_buffer = ToString(payload);
		check_header = check_header_buffer.c_str();
		break;

	case TranslationCommand::WANT:
		want_buffer = FromBytesFloor<const TranslationCommand>(payload);
		want = want_buffer;
		break;

	case TranslationCommand::WANT_FULL_URI:
		want_full_uri_buffer = ToString(payload);
		want_full_uri = AsBytes(want_full_uri_buffer);
		break;

	case TranslationCommand::FILE_NOT_FOUND:
		file_not_found_buffer = ToString(payload);
		file_not_found = AsBytes(file_not_found_buffer);
		break;

	case TranslationCommand::CONTENT_TYPE_LOOKUP:
		content_type_lookup_buffer = ToString(payload);
		content_type_lookup = AsBytes(content_type_lookup_buffer);
		break;

	case TranslationCommand::SUFFIX:
		suffix_buffer = ToString(payload);
		suffix = suffix_buffer.c_str();
		break;

	case TranslationCommand::DIRECTORY_INDEX:
		directory_index_buffer = ToString(payload);
		directory_index = AsBytes(directory_index_buffer);
		break;

	case TranslationCommand::ENOTDIR_:
		enotdir_buffer = ToString(payload);
		enotdir = AsBytes(enotdir_buffer);
		break;

	case TranslationCommand::AUTH:
		auth_buffer = ToString(payload);
		auth = AsBytes(auth_buffer);
		break;

	case TranslationCommand::PROBE_PATH_SUFFIXES:
		probe_path_suffixes_buffer = ToString(payload);
		probe_path_suffixes = AsBytes(probe_path_suffixes_buffer);
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
		read_file = AsBytes(read_file_buffer);
		break;

	case TranslationCommand::INTERNAL_REDIRECT:
		internal_redirect_buffer = ToString(payload);
		internal_redirect = AsBytes(internal_redirect_buffer);
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

	case TranslationCommand::EXECUTE:
		execute_buffer = ToString(payload);
		execute = execute_buffer.c_str();
		break;

	case TranslationCommand::SERVICE:
		service_buffer = ToString(payload);
		service = service_buffer.c_str();
		break;

	case TranslationCommand::CHAIN:
		chain_buffer = ToString(payload);
		chain = AsBytes(chain_buffer);
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
		layout = AsBytes(layout_buffer.data());
		break;

	case TranslationCommand::BASE:
		base_buffer = ToString(payload);
		base = base_buffer.c_str();
		break;

	case TranslationCommand::REGEX:
		regex_buffer = ToString(payload);
		regex = regex_buffer.c_str();
		break;

	case TranslationCommand::PLAN:
		plan_buffer = ToString(payload);
		plan = plan_buffer.c_str();
		break;

	case TranslationCommand::PATH_EXISTS:
		path_exists = true;
		break;

	default:
		throw FormatRuntimeError("unknown translation packet: %u",
					 unsigned(cmd));
	}
}

} // namespace Translation::Server
