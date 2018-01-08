/*
 * Copyright 2007-2017 Content Management AG
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

#include "Connection.hxx"
#include "Error.hxx"

#include <string.h>

namespace Pg {

void
Connection::Connect(const char *conninfo)
{
	assert(!IsDefined());

	conn = ::PQconnectdb(conninfo);
	if (conn == nullptr)
		throw std::bad_alloc();

	if (GetStatus() != CONNECTION_OK)
		throw std::runtime_error(GetErrorMessage());
}

void
Connection::StartConnect(const char *conninfo)
{
	assert(!IsDefined());

	conn = ::PQconnectStart(conninfo);
	if (conn == nullptr)
		throw std::bad_alloc();

	if (GetStatus() == CONNECTION_BAD)
		throw std::runtime_error(GetErrorMessage());
}

void
Connection::ExecuteOrThrow(const char *query)
{
	auto result = Execute(query);
	if (!result.IsCommandSuccessful())
		throw Error(std::move(result));
}

void
Connection::SetRole(const char *role_name)
{
	std::string sql = "SET ROLE '" + Escape(role_name) + "'";
	ExecuteOrThrow(sql.c_str());
}

void
Connection::SetSchema(const char *schema)
{
	std::string sql = "SET SCHEMA '" + Escape(schema) + "'";
	ExecuteOrThrow(sql.c_str());
}

void
Connection::SendQuery(const char *query)
{
	assert(IsDefined());
	assert(query != nullptr);

	if (::PQsendQuery(conn, query) == 0)
		throw std::runtime_error(GetErrorMessage());
}

void
Connection::_SendQuery(bool result_binary, const char *query,
		       size_t n_params, const char *const*values,
		       const int *lengths, const int *formats)
{
	assert(IsDefined());
	assert(query != nullptr);

	if (::PQsendQueryParams(conn, query, n_params, nullptr,
				values, lengths, formats, result_binary) == 0)
		throw std::runtime_error(GetErrorMessage());
}

std::string
Connection::Escape(const char *p, size_t length) const noexcept
{
	assert(p != nullptr || length == 0);

	char *buffer = new char[length * 2 + 1];

	::PQescapeStringConn(conn, buffer, p, length, nullptr);
	std::string result(buffer, length);
	delete[] buffer;
	return result;
}

std::string
Connection::Escape(const char *p) const noexcept
{
	assert(p != nullptr);

	return Escape(p, strlen(p));
}

} /* namespace Pg */
