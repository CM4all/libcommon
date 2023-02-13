// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "ParamWrapper.hxx"
#include "DynamicParamWrapper.hxx"
#include "Result.hxx"
#include "Notify.hxx"
#include "Error.hxx"
#include "util/Concepts.hxx"

#include <libpq-fe.h>

#include <new>
#include <memory>
#include <string>
#include <cassert>
#include <stdexcept>

namespace Pg {

/**
 * A thin C++ wrapper for a PGconn pointer.
 */
class Connection {
	PGconn *conn = nullptr;

public:
	Connection() = default;

	Connection(const char *conninfo) {
		try {
			Connect(conninfo);
		} catch (...) {
			Disconnect();
			throw;
		}
	}

	Connection(const Connection &other) = delete;

	Connection(Connection &&other) noexcept
		:conn(std::exchange(other.conn, nullptr)) {}

	Connection &operator=(const Connection &other) = delete;

	Connection &operator=(Connection &&other) noexcept {
		std::swap(conn, other.conn);
		return *this;
	}

	~Connection() noexcept {
		Disconnect();
	}

	bool IsDefined() const noexcept {
		return conn != nullptr;
	}

	[[gnu::pure]]
	ConnStatusType GetStatus() const noexcept {
		assert(IsDefined());

		return ::PQstatus(conn);
	}

	[[gnu::pure]]
	const char *GetErrorMessage() const noexcept {
		assert(IsDefined());

		return ::PQerrorMessage(conn);
	}

	[[gnu::pure]]
	int GetProtocolVersion() const noexcept {
		assert(IsDefined());

		return ::PQprotocolVersion (conn);
	}

	[[gnu::pure]]
	int GetServerVersion() const noexcept {
		assert(IsDefined());

		return ::PQserverVersion (conn);
	}

	[[gnu::pure]]
	const char *GetParameterStatus(const char *name) const noexcept {
		assert(IsDefined());

		return PQparameterStatus(conn, name);
	}

	[[gnu::pure]]
	int GetBackendPID() const noexcept {
		assert(IsDefined());

		return ::PQbackendPID (conn);
	}

	[[gnu::pure]]
	int GetSocket() const noexcept {
		assert(IsDefined());

		return ::PQsocket(conn);
	}

	void Disconnect() noexcept {
		if (conn != nullptr) {
			::PQfinish(conn);
			conn = nullptr;
		}
	}

	void Connect(const char *conninfo);
	void StartConnect(const char *conninfo);

	PostgresPollingStatusType PollConnect() {
		assert(IsDefined());

		return ::PQconnectPoll(conn);
	}

	void Reconnect() noexcept {
		assert(IsDefined());

		::PQreset(conn);
	}

	void StartReconnect() noexcept {
		assert(IsDefined());

		::PQresetStart(conn);
	}

	PostgresPollingStatusType PollReconnect() noexcept {
		assert(IsDefined());

		return ::PQresetPoll(conn);
	}

	void ConsumeInput() noexcept {
		assert(IsDefined());

		::PQconsumeInput(conn);
	}

	Notify GetNextNotify() noexcept {
		assert(IsDefined());

		return Notify(::PQnotifies(conn));
	}

private:
	/**
	 * Check if the #Result contains an error state, and throw a
	 * #Error based on this condition.  If the #Result did not contain
	 * an error, it is returned as-is.
	 */
	Result CheckError(Result &&result) {
		if (result.IsError())
			throw Error(std::move(result));

		return std::move(result);
	}

protected:
	Result CheckResult(PGresult *result) {
		if (result == nullptr) {
			if (GetStatus() == CONNECTION_BAD)
				throw std::runtime_error(GetErrorMessage());

			throw std::bad_alloc();
		}

		return CheckError(Result(result));
	}

	static size_t CountDynamic() noexcept {
		return 0;
	}

	template<typename T, typename... Params>
	static size_t CountDynamic(const T &t,
				   const Params&... params) noexcept {
		return DynamicParamWrapper<T>::Count(t) +
			CountDynamic(params...);
	}

	Result ExecuteDynamic2(const char *query,
			       const char *const*values,
			       const int *lengths, const int *formats,
			       unsigned n) {
		assert(IsDefined());
		assert(query != nullptr);

		return CheckResult(::PQexecParams(conn, query, n,
						  nullptr, values, lengths, formats,
						  false));
	}

	template<typename T, typename... Params>
	Result ExecuteDynamic2(const char *query,
			       const char **values,
			       int *lengths, int *formats,
			       unsigned n,
			       const T &t, const Params&... params) {
		assert(IsDefined());
		assert(query != nullptr);

		const DynamicParamWrapper<T> w(t);
		n += w.Fill(values + n, lengths + n, formats + n);

		return ExecuteDynamic2(query, values, lengths, formats,
				       n, params...);
	}

public:
	Result Execute(const char *query) {
		assert(IsDefined());
		assert(query != nullptr);

		return CheckResult(::PQexec(conn, query));
	}

	Result Execute(bool result_binary, const char *query) {
		assert(IsDefined());
		assert(query != nullptr);

		return CheckResult(::PQexecParams(conn, query, 0,
						  nullptr, nullptr,
						  nullptr, nullptr,
						  result_binary));
	}

	template<ParamArray A>
	Result ExecuteParams(bool result_binary, const char *query,
			     const A &params) {
		return CheckResult(::PQexecParams(conn, query, params.size(),
						  nullptr, params.GetValues(),
						  params.GetLengths(),
						  params.GetFormats(),
						  result_binary));
	}

	template<typename... Params>
	Result ExecuteParams(bool result_binary,
			     const char *query, const Params&... _params) {
		assert(IsDefined());
		assert(query != nullptr);

		const AutoParamArray<Params...> params(_params...);
		return ExecuteParams(result_binary, query, params);
	}

	template<typename... Params>
	Result ExecuteParams(const char *query, const Params&... params) {
		return ExecuteParams(false, query, params...);
	}

	/**
	 * Execute with dynamic parameter list: this variant of
	 * ExecuteParams() allows std::vector arguments which get
	 * expanded.
	 */
	template<typename... Params>
	Result ExecuteDynamic(const char *query, const Params&... params) {
		assert(IsDefined());
		assert(query != nullptr);

		const size_t n = CountDynamic(params...);
		const auto values = std::make_unique<const char *[]>(n);
		const auto lengths = std::make_unique<int[]>(n);
		const auto formats = std::make_unique<int[]>(n);

		return ExecuteDynamic2<Params...>(query, values.get(),
						  lengths.get(), formats.get(), 0,
						  params...);
	}

	/**
	 * Wrapper for "SET ROLE ...".
	 *
	 * Throws #Error on error.
	 */
	void SetRole(const char *role_name);

	/**
	 * Set the search path to the specified schema.
	 *
	 * Throws #Error on error.
	 */
	void SetSchema(const char *schema);

	/**
	 * Begin a transaction with isolation level "SERIALIZABLE".
	 *
	 * Throws #Error on error.
	 */
	void BeginSerializable() {
		Execute("BEGIN ISOLATION LEVEL SERIALIZABLE");
	}

	/**
	 * Begin a transaction with isolation level "REPEATABLE READ".
	 *
	 * Throws #Error on error.
	 */
	void BeginRepeatableRead() {
		Execute("BEGIN ISOLATION LEVEL REPEATABLE READ");
	}

	/**
	 * Commit the current transaction.
	 *
	 * Throws #Error on error.
	 */
	void Commit() {
		Execute("COMMIT");
	}

	/**
	 * Roll back the current transaction.
	 *
	 * Throws #Error on error.
	 */
	void Rollback() {
		Execute("ROLLBACK");
	}

	/**
	 * Invoke the given function from within a "SERIALIZABLE"
	 * transaction.  Performs automatic rollback if the function
	 * throws an exception.
	 */
	void DoSerializable(Invocable<> auto f) {
		BeginSerializable();

		try {
			f();
		} catch (...) {
			if (IsDefined())
				Rollback();
			throw;
		}

		Commit();
	}

	/**
	 * Invoke the given function from within a "REPEATABLE READ"
	 * transaction.  Performs automatic rollback if the function
	 * throws an exception.
	 */
	void DoRepeatableRead(Invocable<> auto f) {
		BeginRepeatableRead();

		try {
			f();
		} catch (...) {
			if (IsDefined())
				Rollback();
			throw;
		}

		Commit();
	}

	[[gnu::pure]]
	bool IsBusy() const noexcept {
		assert(IsDefined());

		return ::PQisBusy(conn) != 0;
	}

	void SendQuery(const char *query);

	void SendQueryParams(bool result_binary, const char *query,
			     size_t n_params, const char *const*values,
			     const int *lengths, const int *formats);

	template<ParamArray A>
	void SendQuery(bool result_binary, const char *query,
		       const A &params) {
		SendQueryParams(result_binary, query, params.size(),
				params.GetValues(), params.GetLengths(),
				params.GetFormats());
	}

	template<typename... Params>
	void SendQuery(bool result_binary,
		       const char *query, const Params&... _params) {
		assert(IsDefined());
		assert(query != nullptr);

		const AutoParamArray<Params...> params(_params...);
		SendQuery(result_binary, query, params);
	}

	template<typename... Params>
	void SendQuery(const char *query, const Params&... params) {
		SendQuery(false, query, params...);
	}

	void SetSingleRowMode() noexcept {
		PQsetSingleRowMode(conn);
	}

	Result ReceiveResult() noexcept {
		assert(IsDefined());

		return Result(PQgetResult(conn));
	}

	bool RequestCancel() noexcept {
		assert(IsDefined());

		return PQrequestCancel(conn) == 1;
	}

	[[gnu::pure]]
	std::string Escape(std::string_view src) const noexcept;
};

} /* namespace Pg */
