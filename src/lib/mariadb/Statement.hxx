// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <mysql.h>

#include <string_view>
#include <utility>

class MysqlResult;

class MysqlStatement {
	MYSQL_STMT *stmt = nullptr;

public:
	[[nodiscard]]
	MysqlStatement() noexcept = default;

	[[nodiscard]]
	explicit MysqlStatement(MYSQL &mysql);

	MysqlStatement(MysqlStatement &&src) noexcept
		:stmt(std::exchange(src.stmt, nullptr)) {}

	~MysqlStatement() noexcept {
		if (stmt != nullptr)
			mysql_stmt_close(stmt); // This implies mysql_stmt_free_result
	}

	void FreeResult() noexcept {
		mysql_stmt_free_result(stmt);
	}

	MysqlStatement &operator=(MysqlStatement &&src) noexcept {
		using std::swap;
		swap(stmt, src.stmt);
		return *this;
	}

	operator bool() const noexcept {
		return stmt != nullptr;
	}

	/* Note that you must not `Prepare` a query (from any MysqlStatement) is in progress.
	 * First you must either fetch all rows manually, call `FetchAll`, `FreeResult` or `Free`.
	 * Destroying the MysqlStatement in progress is also enough, as it calls `Free`.
	 */
	void Prepare(std::string_view sql);

	[[gnu::pure]]
	std::size_t GetParamCount() const noexcept {
		return mysql_stmt_param_count(stmt);
	}

	[[gnu::pure]]
	std::size_t GetFieldCount() const noexcept {
		return mysql_stmt_field_count(stmt);
	}

	uint64_t GetAffectedRows() const noexcept {
		return mysql_stmt_affected_rows(stmt);
	}

	void BindParam(const MYSQL_BIND *bind);

	void Execute();

	void Execute(const MYSQL_BIND *bind)
	{
		BindParam(bind);
		Execute();
	}

	void StoreResult();

	MysqlResult ResultMetadata();

	void BindResult(const MYSQL_BIND *bind);

	bool Fetch();

	void FetchAll();

	void FetchColumn(MYSQL_BIND &bind, unsigned int column,
			 unsigned long offset=0);
};
