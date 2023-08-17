// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Error.hxx"

#include <mysql.h>

#include <fmt/core.h>

MysqlError::MysqlError(struct st_mysql &connection,
		       const char *prefix) noexcept
	:std::runtime_error(fmt::format("{}: {}", prefix,
					mysql_error(&connection))),
	 code(mysql_errno(&connection))
{
}

MysqlError::MysqlError(struct st_mysql_stmt &stmt,
		       const char *prefix) noexcept
	:std::runtime_error(fmt::format("{}: {}", prefix,
					mysql_stmt_error(&stmt))),
	 code(mysql_stmt_errno(&stmt))
{
}
