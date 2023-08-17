// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <stdexcept>

class MysqlError : public std::runtime_error {
	int code;

public:
	[[nodiscard]]
	MysqlError(const char *msg, int _errno) noexcept
		:std::runtime_error(msg), code(_errno) {}

	[[nodiscard]]
	MysqlError(struct st_mysql &connection, const char *prefix) noexcept;

	[[nodiscard]]
	MysqlError(struct st_mysql_stmt &stmt, const char *prefix) noexcept;
};
