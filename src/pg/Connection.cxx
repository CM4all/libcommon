// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"

#include <cstring>

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
Connection::SetRole(const char *role_name)
{
	std::string sql = "SET ROLE '" + Escape(role_name) + "'";
	Execute(sql.c_str());
}

void
Connection::SetSchema(const char *schema)
{
	std::string sql = "SET SCHEMA '" + Escape(schema) + "'";
	Execute(sql.c_str());
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
Connection::SendQueryParams(bool result_binary, const char *query,
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
Connection::Escape(const std::string_view src) const noexcept
{
	auto buffer = std::make_unique_for_overwrite<char[]>(src.size() * 2 + 1);

	size_t dest_length = ::PQescapeStringConn(conn, buffer.get(),
						  src.data(), src.size(),
						  nullptr);
	return {buffer.get(), dest_length};
}

} /* namespace Pg */
