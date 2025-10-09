// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "pg/SharedConnection.hxx"
#include "event/Loop.hxx"

#include <gtest/gtest.h>

#include <array>
#include <cassert>

#include <sys/socket.h>

namespace {

struct Handler final : Pg::SharedConnectionHandler {
	std::exception_ptr error;

	void OnPgError(std::exception_ptr _error) noexcept override {
		assert(!error);
		assert(_error);
		error = std::move(_error);
	}
};

static void
BreakConnection(SocketDescriptor s)
{
	s.ShutdownRead();

	std::array<std::byte, 4096> buffer;
	(void)s.ReadNoWait(buffer);
}

struct Query final : Pg::SharedConnectionQuery, Pg::AsyncResultHandler {
	const char *query = "SELECT 1";

	DeferEvent defer_cancel_event{GetEventLoop(), BIND_THIS_METHOD(Cancel)};

	Pg::Result result;
	std::exception_ptr error;

	enum class State {
		INIT,
		SEND,
		END,
		ERROR,
	} state = State::INIT;

	bool cancel = false, defer_cancel = false, disconnect = false, quit = false;

	using Pg::SharedConnectionQuery::SharedConnectionQuery;

	void OnPgConnectionAvailable(Pg::AsyncConnection &connection) override {
		assert(!error);
		assert(state == State::INIT);

		state = State::SEND;
		connection.SendQuery(*this, query);

		if (cancel)
			Cancel();
		else if (defer_cancel)
			defer_cancel_event.Schedule();

		if (disconnect)
			BreakConnection(SocketDescriptor{connection.GetSocket()});
	}

	void OnPgError(std::exception_ptr _error) noexcept override {
		assert(!error);
		error = std::move(_error);

		if (quit)
			GetEventLoop().Break();
	}

	void OnResult(Pg::Result &&_result) override {
		assert(!result.IsDefined());
		assert(_result.IsDefined());
		assert(state == State::SEND);

		result = std::move(_result);
	}

	void OnResultEnd() override {
		assert(state == State::SEND);

		state = State::END;

		if (quit)
			GetEventLoop().Break();
	}

	void OnResultError() noexcept override {
		assert(state == State::SEND);

		state = State::ERROR;

		if (quit)
			GetEventLoop().Break();
	}
};

} // anonymous namespace

TEST(SharedConnection, One)
{
	const char *conninfo = getenv("PG_CONNINFO");
	if (conninfo == nullptr) {
		GTEST_SKIP();
	}

	const char *schema = getenv("PG_SCHEMA");
	if (schema == nullptr)
		schema = "";

	EventLoop event_loop;
	Handler handler;
	Pg::SharedConnection connection{event_loop, conninfo, schema, handler};

	std::array queries{Query{connection}};
	queries.back().quit = true;

	for (auto &i : queries)
		connection.ScheduleQuery(i);

	for (const auto &i : queries) {
		EXPECT_FALSE(i.result.IsDefined());
		EXPECT_EQ(i.state, Query::State::INIT);
	}

	EXPECT_FALSE(handler.error);

	event_loop.Run();

	for (const auto &i : queries) {
		EXPECT_TRUE(i.result.IsDefined());
		EXPECT_EQ(i.state, Query::State::END);
	}

	EXPECT_FALSE(handler.error);

	if (handler.error)
		std::rethrow_exception(handler.error);
}

TEST(SharedConnection, Serial)
{
	const char *conninfo = getenv("PG_CONNINFO");
	if (conninfo == nullptr) {
		GTEST_SKIP();
	}

	const char *schema = getenv("PG_SCHEMA");
	if (schema == nullptr)
		schema = "";

	EventLoop event_loop;
	Handler handler;
	Pg::SharedConnection connection{event_loop, conninfo, schema, handler};

	for (unsigned i = 0; i < 4; ++i) {
		Query query{connection};
		query.quit = true;
		connection.ScheduleQuery(query);

		EXPECT_FALSE(query.result.IsDefined());
		EXPECT_EQ(query.state, Query::State::INIT);
		EXPECT_FALSE(handler.error);

		event_loop.Run();

		EXPECT_TRUE(query.result.IsDefined());
		EXPECT_EQ(query.state, Query::State::END);
		EXPECT_FALSE(handler.error);
	}

	if (handler.error)
		std::rethrow_exception(handler.error);
}

TEST(SharedConnection, Multi)
{
	const char *conninfo = getenv("PG_CONNINFO");
	if (conninfo == nullptr) {
		GTEST_SKIP();
	}

	const char *schema = getenv("PG_SCHEMA");
	if (schema == nullptr)
		schema = "";

	EventLoop event_loop;
	Handler handler;
	Pg::SharedConnection connection{event_loop, conninfo, schema, handler};

	std::array queries{Query{connection}, Query{connection}, Query{connection}, Query{connection}};
	queries.back().quit = true;

	for (auto &i : queries)
		connection.ScheduleQuery(i);

	for (const auto &i : queries) {
		EXPECT_FALSE(i.result.IsDefined());
		EXPECT_EQ(i.state, Query::State::INIT);
	}

	EXPECT_FALSE(handler.error);

	event_loop.Run();

	for (const auto &i : queries) {
		EXPECT_TRUE(i.result.IsDefined());
		EXPECT_EQ(i.state, Query::State::END);
	}

	EXPECT_FALSE(handler.error);

	if (handler.error)
		std::rethrow_exception(handler.error);
}

TEST(SharedConnection, Cancel)
{
	const char *conninfo = getenv("PG_CONNINFO");
	if (conninfo == nullptr) {
		GTEST_SKIP();
	}

	const char *schema = getenv("PG_SCHEMA");
	if (schema == nullptr)
		schema = "";

	EventLoop event_loop;
	Handler handler;
	Pg::SharedConnection connection{event_loop, conninfo, schema, handler};

	std::array queries{Query{connection}, Query{connection}, Query{connection}, Query{connection}};
	queries[1].cancel = true;
	queries[2].cancel = true;
	queries.back().quit = true;

	for (auto &i : queries)
		connection.ScheduleQuery(i);

	for (const auto &i : queries) {
		EXPECT_FALSE(i.result.IsDefined());
		EXPECT_EQ(i.state, Query::State::INIT);
	}

	EXPECT_FALSE(handler.error);

	event_loop.Run();

	if (handler.error)
		std::rethrow_exception(handler.error);
}

TEST(SharedConnection, DeferCancel)
{
	const char *conninfo = getenv("PG_CONNINFO");
	if (conninfo == nullptr) {
		GTEST_SKIP();
	}

	const char *schema = getenv("PG_SCHEMA");
	if (schema == nullptr)
		schema = "";

	EventLoop event_loop;
	Handler handler;
	Pg::SharedConnection connection{event_loop, conninfo, schema, handler};

	std::array queries{Query{connection}, Query{connection}, Query{connection}, Query{connection}};
	queries[1].defer_cancel = true;
	queries[2].defer_cancel = true;
	queries.back().quit = true;

	for (auto &i : queries)
		connection.ScheduleQuery(i);

	for (const auto &i : queries) {
		EXPECT_FALSE(i.result.IsDefined());
		EXPECT_EQ(i.state, Query::State::INIT);
	}

	EXPECT_FALSE(handler.error);

	event_loop.Run();

	if (handler.error)
		std::rethrow_exception(handler.error);
}

TEST(SharedConnection, CancelSleep)
{
	const char *conninfo = getenv("PG_CONNINFO");
	if (conninfo == nullptr) {
		GTEST_SKIP();
	}

	const char *schema = getenv("PG_SCHEMA");
	if (schema == nullptr)
		schema = "";

	EventLoop event_loop;
	Handler handler;
	Pg::SharedConnection connection{event_loop, conninfo, schema, handler};

	std::array queries{Query{connection}, Query{connection}, Query{connection}};

	for (auto &i : queries) {
		i.query = "SELECT pg_sleep(10)";
		i.defer_cancel = true;
		connection.ScheduleQuery(i);
	}

	queries.back().query = "SELECT 1";
	queries.back().defer_cancel = false;
	queries.back().quit = true;

	for (const auto &i : queries) {
		EXPECT_FALSE(i.result.IsDefined());
		EXPECT_EQ(i.state, Query::State::INIT);
	}

	EXPECT_FALSE(handler.error);

	event_loop.Run();

	if (handler.error)
		std::rethrow_exception(handler.error);
}

TEST(SharedConnection, Disconnect)
{
	const char *conninfo = getenv("PG_CONNINFO");
	if (conninfo == nullptr) {
		GTEST_SKIP();
	}

	const char *schema = getenv("PG_SCHEMA");
	if (schema == nullptr)
		schema = "";

	EventLoop event_loop;
	Handler handler;
	Pg::SharedConnection connection{event_loop, conninfo, schema, handler};

	std::array queries{Query{connection}, Query{connection}};
	queries[1].disconnect = true;

	Query last_query{connection};
	last_query.quit = true;

	for (auto &i : queries)
		connection.ScheduleQuery(i);
	connection.ScheduleQuery(last_query);

	for (const auto &i : queries) {
		EXPECT_FALSE(i.result.IsDefined());
		EXPECT_EQ(i.state, Query::State::INIT);
	}

	EXPECT_FALSE(last_query.result.IsDefined());
	EXPECT_EQ(last_query.state, Query::State::INIT);
	EXPECT_FALSE(handler.error);

	event_loop.Run();

	EXPECT_TRUE(queries[0].result.IsDefined());
	EXPECT_EQ(queries[0].state, Query::State::END);

	EXPECT_FALSE(queries[1].result.IsDefined());
	EXPECT_EQ(queries[1].state, Query::State::ERROR);

	EXPECT_TRUE(last_query.result.IsDefined());
	EXPECT_EQ(last_query.state, Query::State::END);

	EXPECT_FALSE(handler.error);

	if (handler.error)
		std::rethrow_exception(handler.error);
}
