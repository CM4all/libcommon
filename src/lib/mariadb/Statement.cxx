// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Statement.hxx"
#include "Result.hxx"
#include "Error.hxx"

#include <new> // for std::bad_alloc
#include <stdexcept>

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
		throw MysqlError{*stmt, "mysql_stmt_prepare() failed"};
}

void
MysqlStatement::BindParam(const MYSQL_BIND *bind)
{
	// This const_cast should be fine, because mysql_stmt_bind_param copies all the data in bind
	// to an internal buffer.
	if (mysql_stmt_bind_param(stmt, const_cast<MYSQL_BIND*>(bind)) != 0)
		throw MysqlError{*stmt, "mysql_stmt_bind_param() failed"};
}

void
MysqlStatement::Execute()
{
	if (mysql_stmt_execute(stmt) != 0)
		throw MysqlError{*stmt, "mysql_stmt_execute() failed"};
}

void
MysqlStatement::StoreResult()
{
	if (mysql_stmt_store_result(stmt) != 0)
		throw MysqlError{*stmt, "mysql_stmt_store_result() failed"};
}

MysqlResult
MysqlStatement::ResultMetadata()
{
	MYSQL_RES *result = mysql_stmt_result_metadata(stmt);
	if (result == nullptr) {
		if (mysql_stmt_errno(stmt) == 0)
			throw std::runtime_error("Query can not return a result");
		else
			throw MysqlError{*stmt, "mysql_stmt_result_metadata() failed"};
	}

	return MysqlResult{result};
}

void
MysqlStatement::BindResult(const MYSQL_BIND *bind)
{
	if (mysql_stmt_bind_result(stmt, const_cast<MYSQL_BIND*>(bind)) != 0)
		throw MysqlError{*stmt, "mysql_stmt_bind_result() failed"};
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
		throw MysqlError{*stmt, "mysql_stmt_fetch() failed"};
	}
}

void MysqlStatement::FetchAll()
{
	while (mysql_stmt_fetch(stmt) == 0);
}

void
MysqlStatement::FetchColumn(MYSQL_BIND &bind, unsigned int column,
			    unsigned long offset)
{
	if (mysql_stmt_fetch_column(stmt, &bind, column, offset) != 0)
		throw MysqlError{*stmt, "mysql_stmt_fetch_column() failed"};
}
