// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <mysql.h>

#include <string_view>

class MysqlStatement;
class MysqlResult;

class MysqlConnection {
	MYSQL mysql;

public:
	MysqlConnection() noexcept {
		mysql_init(&mysql);
	}

	~MysqlConnection() noexcept {
		mysql_close(&mysql);
	}

	MysqlConnection(const MysqlConnection &) = delete;
	MysqlConnection &operator=(const MysqlConnection &) = delete;

	void SetOption(enum mysql_option option, const void *arg) noexcept {
		mysql_options(&mysql, option, arg);
	}

	void Connect(const char *host, const char *user, const char *passwd,
		     const char *db, unsigned int port=3306,
		     const char *unix_socket=nullptr,
		     unsigned long clientflag=0);

	void Query(std::string_view sql);

	MysqlResult StoreResult();

	[[gnu::pure]]
	bool HasMoreResults() noexcept {
		return mysql_more_results(&mysql);
	}

	bool NextResult();

	MysqlStatement Prepare(std::string_view sql);
};
