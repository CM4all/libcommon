// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Statement.hxx"
#include "Result.hxx"
#include "util/RuntimeError.hxx"

#include <new> // for std::bad_alloc

MysqlStatement::MysqlStatement(MYSQL &mysql)
	:stmt(mysql_stmt_init(&mysql))
{
	if (stmt == nullptr)
		throw std::bad_alloc{};
}

void
MysqlStatement::Prepare(std::string_view sql)
{
	if (mysql_stmt_prepare(stmt, sql.data(), sql.size()) != 0)
		throw FormatRuntimeError("mysql_stmt_prepare() failed: %s",
					 mysql_stmt_error(stmt));
}

void
MysqlStatement::BindParam(MYSQL_BIND *bnd)
{
	if (mysql_stmt_bind_param(stmt, bnd) != 0)
		throw FormatRuntimeError("mysql_stmt_bind_param() failed: %s",
					 mysql_stmt_error(stmt));
}

void
MysqlStatement::Execute()
{
	if (mysql_stmt_execute(stmt) != 0)
		throw FormatRuntimeError("mysql_stmt_execute() failed: %s",
					 mysql_stmt_error(stmt));
}

void
MysqlStatement::StoreResult()
{
	if (mysql_stmt_store_result(stmt) != 0)
		throw FormatRuntimeError("mysql_stmt_store_result() failed: %s",
					 mysql_stmt_error(stmt));
}

MysqlResult
MysqlStatement::ResultMetadata()
{
	MYSQL_RES *result = mysql_stmt_result_metadata(stmt);
	if (result == nullptr) {
		if (mysql_stmt_errno(stmt) == 0)
			throw std::runtime_error("Query can not return a result");
		else
			throw FormatRuntimeError("mysql_stmt_result_metadata() failed: %s",
						 mysql_stmt_error(stmt));
	}

	return MysqlResult{result};
}

void
MysqlStatement::BindResult(MYSQL_BIND *bnd)
{
	if (mysql_stmt_bind_result(stmt, bnd) != 0)
		throw FormatRuntimeError("mysql_stmt_bind_result() failed: %s",
					 mysql_stmt_error(stmt));
}

bool
MysqlStatement::Fetch()
{
	switch (mysql_stmt_fetch(stmt)) {
	case 0:
	case MYSQL_DATA_TRUNCATED:
		return true;

	case MYSQL_NO_DATA:
		return false;

	default:
		throw FormatRuntimeError("mysql_stmt_fetch() failed: %s",
					 mysql_stmt_error(stmt));
	}
}

void
MysqlStatement::FetchColumn(MYSQL_BIND &bind, unsigned int column,
			    unsigned long offset)
{
	if (mysql_stmt_fetch_column(stmt, &bind, column, offset) != 0)
		throw FormatRuntimeError("mysql_stmt_fetch_column() failed: %s",
					 mysql_stmt_error(stmt));
}
