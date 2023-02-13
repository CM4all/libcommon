// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Result.hxx"
#include "util/RuntimeError.hxx"

#include <mysql.h>

#include <string_view>
#include <utility>

class MysqlResult;

class MysqlStatement {
	MYSQL_STMT *stmt = nullptr;

public:
	MysqlStatement() noexcept = default;

	explicit MysqlStatement(MYSQL &mysql)
		:stmt(mysql_stmt_init(&mysql))
	{
		if (stmt == nullptr)
			throw std::bad_alloc{};
	}

	MysqlStatement(MysqlStatement &&src) noexcept
		:stmt(std::exchange(src.stmt, nullptr)) {}

	~MysqlStatement() noexcept {
		if (stmt != nullptr)
			mysql_stmt_close(stmt);
	}

	MysqlStatement &operator=(MysqlStatement &&src) noexcept {
		using std::swap;
		swap(stmt, src.stmt);
		return *this;
	}

	operator bool() const noexcept {
		return stmt != nullptr;
	}

	void Prepare(std::string_view sql) {
		if (mysql_stmt_prepare(stmt, sql.data(), sql.size()) != 0)
			throw FormatRuntimeError("mysql_stmt_prepare() failed: %s",
						 mysql_stmt_error(stmt));
	}

	[[gnu::pure]]
	std::size_t GetParamCount() const noexcept {
		return mysql_stmt_param_count(stmt);
	}

	[[gnu::pure]]
	std::size_t GetFieldCount() const noexcept {
		return mysql_stmt_field_count(stmt);
	}

	void BindParam(MYSQL_BIND *bnd) {
		if (mysql_stmt_bind_param(stmt, bnd) != 0)
			throw FormatRuntimeError("mysql_stmt_bind_param() failed: %s",
						 mysql_stmt_error(stmt));
	}

	void Execute() {
		if (mysql_stmt_execute(stmt) != 0)
			throw FormatRuntimeError("mysql_stmt_execute() failed: %s",
						 mysql_stmt_error(stmt));
	}

	void StoreResult() {
		if (mysql_stmt_store_result(stmt) != 0)
			throw FormatRuntimeError("mysql_stmt_store_result() failed: %s",
						 mysql_stmt_error(stmt));
	}

	MysqlResult ResultMetadata();

	void BindResult(MYSQL_BIND *bnd) {
		if (mysql_stmt_bind_result(stmt, bnd) != 0)
			throw FormatRuntimeError("mysql_stmt_bind_result() failed: %s",
						 mysql_stmt_error(stmt));
	}

	bool Fetch() {
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

	void FetchColumn(MYSQL_BIND &bind, unsigned int column,
			 unsigned long offset=0) {
		if (mysql_stmt_fetch_column(stmt, &bind, column, offset) != 0)
			throw FormatRuntimeError("mysql_stmt_fetch_column() failed: %s",
						 mysql_stmt_error(stmt));
	}
};
