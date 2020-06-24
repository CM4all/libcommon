/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "util/Compiler.h"

#if __cplusplus >= 201703L && !GCC_OLDER_THAN(7,0)
#include <string_view>
#endif

#include <libpq-fe.h>

#include <cassert>
#include <cstdlib>
#include <string>
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

	gcc_pure
	ExecStatusType GetStatus() const noexcept {
		assert(IsDefined());

		return ::PQresultStatus(result);
	}

	gcc_pure
	bool IsCommandSuccessful() const noexcept {
		return GetStatus() == PGRES_COMMAND_OK;
	}

	gcc_pure
	bool IsQuerySuccessful() const noexcept {
		return GetStatus() == PGRES_TUPLES_OK;
	}

	gcc_pure
	bool IsError() const noexcept {
		const auto status = GetStatus();
		return status == PGRES_BAD_RESPONSE ||
			status == PGRES_NONFATAL_ERROR ||
			status == PGRES_FATAL_ERROR;
	}

	gcc_pure
	const char *GetErrorMessage() const noexcept {
		assert(IsDefined());

		return ::PQresultErrorMessage(result);
	}

	gcc_pure
	const char *GetErrorField(int fieldcode) const noexcept {
		assert(IsDefined());

		return PQresultErrorField(result, fieldcode);
	}

	gcc_pure
	const char *GetErrorType() const noexcept {
		assert(IsDefined());

		return GetErrorField(PG_DIAG_SQLSTATE);
	}

	/**
	 * Returns the number of rows that were affected by the command.
	 * The caller is responsible for checking GetStatus().
	 */
	gcc_pure
	unsigned GetAffectedRows() const noexcept {
		assert(IsDefined());
		assert(IsCommandSuccessful());

		return std::strtoul(::PQcmdTuples(result), nullptr, 10);
	}

	/**
	 * Returns true if there are no rows in the result.
	 */
	gcc_pure
	bool IsEmpty() const noexcept {
		assert(IsDefined());

		return ::PQntuples(result) == 0;
	}

	gcc_pure
	unsigned GetRowCount() const noexcept {
		assert(IsDefined());

		return ::PQntuples(result);
	}

	gcc_pure
	unsigned GetColumnCount() const noexcept {
		assert(IsDefined());

		return ::PQnfields(result);
	}

	gcc_pure
	const char *GetColumnName(unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQfname(result, column);
	}

	gcc_pure
	bool IsColumnBinary(unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQfformat(result, column);
	}

	gcc_pure
	Oid GetColumnType(unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQftype(result, column);
	}

	gcc_pure
	bool IsColumnTypeBinary(unsigned column) const noexcept {
		/* 17 = bytea */
		return GetColumnType(column) == 17;
	}

	/**
	 * Obtains the given value, and return an empty string if the
	 * value is NULL.  Call IsValueNull() to find out whether the
	 * real value was NULL or an empty string.
	 */
	gcc_pure
	const char *GetValue(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQgetvalue(result, row, column);
	}

	gcc_pure
	unsigned GetValueLength(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQgetlength(result, row, column);
	}

#if __cplusplus >= 201703L && !GCC_OLDER_THAN(7,0)
	gcc_pure
	std::string_view GetValueView(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return {GetValue(row, column), GetValueLength(row, column)};
	}
#endif

	gcc_pure
	bool IsValueNull(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return ::PQgetisnull(result, row, column);
	}

	/**
	 * Obtains the given value, but return nullptr instead of an
	 * empty string if the value is NULL.
	 */
	gcc_pure
	const char *GetValueOrNull(unsigned row, unsigned column) const noexcept {
		assert(IsDefined());

		return IsValueNull(row, column)
			? nullptr
			: GetValue(row, column);
	}

	gcc_pure
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
	gcc_pure
	std::string GetOnlyStringChecked() const noexcept;

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

		RowIterator &operator*() noexcept {
			return *this;
		}

		gcc_pure
		const char *GetValue(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return ::PQgetvalue(result, row, column);
		}

		gcc_pure
		unsigned GetValueLength(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return ::PQgetlength(result, row, column);
		}

#if __cplusplus >= 201703L && !GCC_OLDER_THAN(7,0)
		gcc_pure
		std::string_view GetValueView(unsigned column) const noexcept {
			return {GetValue(column), GetValueLength(column)};
		}
#endif

		gcc_pure
		bool IsValueNull(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return ::PQgetisnull(result, row, column);
		}

		gcc_pure
		const char *GetValueOrNull(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return IsValueNull(column)
				? nullptr
				: GetValue(column);
		}

		gcc_pure
		BinaryValue GetBinaryValue(unsigned column) const noexcept {
			assert(result != nullptr);
			assert(row < (unsigned)::PQntuples(result));
			assert(column < (unsigned)::PQnfields(result));

			return BinaryValue(GetValue(column), GetValueLength(column));
		}
	};

	typedef RowIterator iterator;

	iterator begin() const noexcept {
		return iterator{result, 0};
	}

	iterator end() const noexcept {
		return iterator{result, GetRowCount()};
	}
};

} /* namespace Pg */
