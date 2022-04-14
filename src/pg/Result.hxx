/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BinaryValue.hxx"

#include <libpq-fe.h>

#include <cassert>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace Pg {

/**
 * A thin C++ wrapper for a PGresult pointer.
 */
class Result {
	PGresult *result;

public:
	Result():result(nullptr) {}
	explicit Result(PGresult *_result):result(_result) {}

	Result(Result &&other) noexcept
		:result(std::exchange(other.result, nullptr)) {}

	~Result() noexcept {
		if (result != nullptr)
			::PQclear(result);
	}

	bool IsDefined() const noexcept {
		return result != nullptr;
	}

	Result &operator=(Result &&other) noexcept {
		using std::swap;
		swap(result, other.result);
		return *this;
	}

	[[gnu::pure]]
	ExecStatusType GetStatus() const noexcept {
		assert(IsDefined());

		return ::PQresultStatus(result);
	}

	[[gnu::pure]]
	bool IsCommandSuccessful() const noexcept {
		return GetStatus() == PGRES_COMMAND_OK;
	}

	[[gnu::pure]]
	bool IsQuerySuccessful() const noexcept {
		return GetStatus() == PGRES_TUPLES_OK;
	}

	[[gnu::pure]]
	bool IsError() const noexcept {
		const auto status = GetStatus();
		return status == PGRES_BAD_RESPONSE ||
			status == PGRES_NONFATAL_ERROR ||
			status == PGRES_FATAL_ERROR;
	}

	[[gnu::pure]]
	const char *GetErrorMessage() const noexcept {
		assert(IsDefined());

		return ::PQresultErrorMessage(result);
	}

	[[gnu::pure]]
	const char *GetErrorField(int fieldcode) const noexcept {
		assert(IsDefined());

		return PQresultErrorField(result, fieldcode);
	}

	[[gnu::pure]]
	const char *GetErrorType() const noexcept {
		assert(IsDefined());

		return GetErrorField(PG_DIAG_SQLSTATE);
	}

	/**
	 * Returns the number of rows that were affected by the command.
	 * The caller is responsible for checking GetStatus().
	 */
	[[gnu::pure]]
	unsigned GetAffectedRows() const noexcept {
		assert(IsDefined());
		assert(IsCommandSuccessful());

		return std::strtoul(::PQcmdTuples(result), nullptr, 10);
	}

	/**
	 * Returns true if there are no rows in the result.
	 */
	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		assert(IsDefined());

		return ::PQntuples(result) == 0;
	}

	[[gnu::pure]]
	unsigned GetRowCount() const noexcept {
		assert(IsDefined());

		return ::PQntuples(result);
	}

	[[gnu::pure]]
	unsigned GetColumnCount() const noexcept {
		assert(IsDefined());

		return ::PQnfields(result);
	}

	[[gnu::pure]]
	const char *GetColumnName(unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQfname(result, column);
	}

	[[gnu::pure]]
	bool IsColumnBinary(unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQfformat(result, column);
	}

	[[gnu::pure]]
	Oid GetColumnType(unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQftype(result, column);
	}

	[[gnu::pure]]
	bool IsColumnTypeBinary(unsigned column) const noexcept {
		/* 17 = bytea */
		return GetColumnType(column) == 17;
	}

	/**
	 * Obtains the given value, and return an empty string if the
	 * value is NULL.  Call IsValueNull() to find out whether the
	 * real value was NULL or an empty string.
	 */
	[[gnu::pure]]
	const char *GetValue(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQgetvalue(result, row, column);
	}

	[[gnu::pure]]
	unsigned GetValueLength(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQgetlength(result, row, column);
	}

	[[gnu::pure]]
	std::string_view GetValueView(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return {GetValue(row, column), GetValueLength(row, column)};
	}

	[[gnu::pure]]
	bool GetBoolValue(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());
		assert(!::PQgetisnull(result, row, column));
		assert(::PQftype(result, column) == 16);

		return ::PQgetvalue(result, row, column)[0] == 't';
	}

	[[gnu::pure]]
	long GetLongValue(unsigned row, unsigned column) const {
		assert(IsDefined());
		assert(!::PQgetisnull(result, row, column));
		assert(::PQftype(result, column) == 20 /* int8 */
			|| ::PQftype(result, column) == 23 /* int4 */
			|| ::PQftype(result, column) == 21 /* int2 */);

		auto s = ::PQgetvalue(result, row, column);
		char *endptr;
		auto value = strtol(s, &endptr, 10);
		assert(endptr != s && *endptr == 0);

		return value;
	}

	[[gnu::pure]]
	bool IsValueNull(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQgetisnull(result, row, column);
	}

	/**
	 * Is at least one of the given values `NULL`?
	 */
	[[gnu::pure]]
	bool IsAnyValueNull(unsigned row,
			    std::initializer_list<unsigned> columns) const noexcept {
		for (unsigned column : columns)
			if (IsValueNull(row, column))
				return true;

		return false;
	}

	/**
	 * Obtains the given value, but return nullptr instead of an
	 * empty string if the value is NULL.
	 */
	[[gnu::pure]]
	const char *GetValueOrNull(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return IsValueNull(row, column)
			? nullptr
			: GetValue(row, column);
	}

	[[gnu::pure]]
	BinaryValue GetBinaryValue(unsigned row, unsigned column) const noexcept {
		assert(IsColumnBinary(column));

		return BinaryValue(GetValue(row, column),
				   GetValueLength(row, column));
	}

	/**
	 * Returns the only value (row 0, column 0) from the result.
	 * Returns an empty string if the result is not valid or if there
	 * is no row or if the value is nullptr.
	 */
	[[gnu::pure]]
	std::string GetOnlyStringChecked() const noexcept;

	class RowIterator;

	class Row {
		friend class Result;
		friend class RowIterator;

		PGresult *result;
		unsigned row;

		constexpr Row(PGresult *_result, unsigned _row) noexcept
			:result(_result), row(_row) {}

	public:
		[[gnu::pure]]
		unsigned GetColumnCount() const noexcept {
			return ::PQnfields(result);
		}

		[[gnu::pure]]
		const char *GetValue(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return ::PQgetvalue(result, row, column);
		}

		[[gnu::pure]]
		unsigned GetValueLength(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return ::PQgetlength(result, row, column);
		}

		[[gnu::pure]]
		std::string_view GetValueView(unsigned column) const noexcept {
			return {GetValue(column), GetValueLength(column)};
		}

		[[gnu::pure]]
		bool GetBoolValue(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));
			assert(::PQftype(result, column) == 16);
			assert(!::PQgetisnull(result, row, column));

			return ::PQgetvalue(result, row, column)[0] == 't';
		}

		[[gnu::pure]]
		long GetLongValue(unsigned column) const {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));
			assert(!::PQgetisnull(result, row, column));
			assert(::PQftype(result, column) == 20 /* int8 */
				|| ::PQftype(result, column) == 23 /* int4 */
				|| ::PQftype(result, column) == 21 /* int2 */);

			auto s = ::PQgetvalue(result, row, column);
			char *endptr;
			auto value = strtol(s, &endptr, 10);
			assert(endptr != s && *endptr == 0);

			return value;
		}

		[[gnu::pure]]
		bool IsValueNull(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return ::PQgetisnull(result, row, column);
		}

		/**
		 * Is at least one of the given values `NULL`?
		 */
		[[gnu::pure]]
		bool IsAnyValueNull(std::initializer_list<unsigned> columns) const noexcept {
			for (unsigned column : columns)
				if (IsValueNull(column))
					return true;

			return false;
		}

		[[gnu::pure]]
		const char *GetValueOrNull(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return IsValueNull(column)
				? nullptr
				: GetValue(column);
		}

		[[gnu::pure]]
		BinaryValue GetBinaryValue(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return BinaryValue(GetValue(column), GetValueLength(column));
		}
	};

	Row GetRow(unsigned row) const noexcept {
		return Row{result, row};
	}

	class RowIterator {
		PGresult *result;
		unsigned row;

	public:
		constexpr RowIterator(PGresult *_result, unsigned _row) noexcept
			:result(_result), row(_row) {}

		constexpr bool operator==(const RowIterator &other) const noexcept {
			return row == other.row;
		}

		constexpr bool operator!=(const RowIterator &other) const noexcept {
			return row != other.row;
		}

		RowIterator &operator++() noexcept {
			++row;
			return *this;
		}

		Row operator*() noexcept {
			return Row(result, row);
		}
	};

	using iterator = RowIterator;

	iterator begin() const noexcept {
		return iterator{result, 0};
	}

	iterator end() const noexcept {
		return iterator{result, GetRowCount()};
	}
};

} /* namespace Pg */
