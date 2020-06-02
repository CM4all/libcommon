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

#include "Result.hxx"

#include <exception>

struct StringView;

namespace Pg {

class Error final : public std::exception {
	Result result;

public:
	Error(const Error &other) = delete;
	Error(Error &&other) noexcept
		:result(std::move(other.result)) {}
	explicit Error(Result &&_result) noexcept
		:result(std::move(_result)) {}

	Error &operator=(const Error &other) = delete;

	Error &operator=(Error &&other) noexcept {
		result = std::move(other.result);
		return *this;
	}

	Error &operator=(Result &&other) noexcept {
		result = std::move(other);
		return *this;
	}

	gcc_pure
	ExecStatusType GetStatus() const noexcept {
		return result.GetStatus();
	}

	gcc_pure
	const char *GetType() const noexcept {
		return result.GetErrorType();
	}

	gcc_pure
	bool IsType(const char *type) const noexcept;

	gcc_pure
	bool HasTypePrefix(StringView type_prefix) const noexcept;

	/**
	 * Is this error fatal, i.e. has the connection become
	 * unusable?
	 */
	gcc_pure
	bool IsFatal() const noexcept;

	/**
	 * Is this a serialization failure, i.e. a problem with "BEGIN
	 * SERIALIZABLE" or Pg::Connection::BeginSerializable().
	 */
	gcc_pure
	bool IsSerializationFailure() const noexcept {
		// https://www.postgresql.org/docs/current/static/errcodes-appendix.html
		return IsType("40001");
	}

	gcc_pure
	const char *what() const noexcept override {
		return result.GetErrorMessage();
	}
};

} /* namespace Pg */
