// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <mysql.h>

#include <utility>

class MysqlResult {
	MYSQL_RES *result = nullptr;

public:
	MysqlResult() noexcept = default;

	explicit MysqlResult(MYSQL_RES *_result) noexcept
		:result(_result) {}

	MysqlResult(MysqlResult &&src) noexcept
		:result(std::exchange(src.result, nullptr)) {}

	~MysqlResult() noexcept {
		if (result != nullptr)
			mysql_free_result(result);
	}

	MysqlResult &operator=(MysqlResult &&src) noexcept {
		using std::swap;
		swap(result, src.result);
		return *this;
	}

	operator bool() const noexcept {
		return result != nullptr;
	}

	const auto *operator->() const noexcept {
		return result;
	}

	MYSQL_ROW FetchRow() const noexcept {
		return mysql_fetch_row(result);
	}

	const unsigned long *FetchLengths() const noexcept {
		return mysql_fetch_lengths(result);
	}
};
