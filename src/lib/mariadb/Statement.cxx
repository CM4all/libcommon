// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Statement.hxx"
#include "Result.hxx"

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
