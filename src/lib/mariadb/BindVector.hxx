// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <mysql.h>

#include <memory>

struct MysqlBindVector {
	std::unique_ptr<MYSQL_BIND[]> binds;
	std::unique_ptr<unsigned long[]> lengths;
	std::unique_ptr<my_bool[]> is_nulls;

	MysqlBindVector() noexcept = default;

	explicit MysqlBindVector(std::size_t size) noexcept
		:binds(new MYSQL_BIND[size]),
		 lengths(new unsigned long[size]),
		 is_nulls(new my_bool[size]) {
		for (std::size_t i = 0; i < size; ++i) {
			binds[i] = {
				.length = &lengths[i],
				.is_null = &is_nulls[i],
			};
		}
	}

	operator MYSQL_BIND *() const noexcept {
		return binds.get();
	}
};
