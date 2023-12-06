// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Result.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lua/Assert.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/Resume.hxx"
#include "lua/Value.hxx"
#include "lua/CoRunner.hxx"
#include "pg/SharedConnection.hxx"
#include "util/AllocatedArray.hxx"
#include "util/Cancellable.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringBuffer.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <forward_list>
#include <map>
#include <string>

#include <stdio.h>

namespace Lua {

class PgRequest;

class PgConnection final : Pg::SharedConnectionHandler {
	Pg::SharedConnection connection;

	/**
	 * A registration of one NOTIFY, submitted to PostgreSQL via
	 * LISTEN.
	 *
	 * @see https://www.postgresql.org/docs/current/sql-listen.html
	 */
	class NotifyRegistration final : ResumeListener {
		/**
		 * The Lua function that will be invoked each time
		 * this NOTIFY is received.
		 */
		Value handler;

		/**
		 * The Lua thread which currently runs the handler
		 * coroutine.
		 */
		CoRunner thread;

		bool busy = false, again = false;

	public:
		/**
		 * Was LISTEN already called on the current PostgreSQL
		 * connection?
		 */
		bool registered = false;

		template<typename H>
		explicit NotifyRegistration(lua_State *L, H &&_handler) noexcept
			:handler(L, std::forward<H>(_handler)), thread(L) {}

		void Start() noexcept;

	private:
		/* virtual methods from class ResumeListener */
		void OnLuaFinished(lua_State *L) noexcept override;
		void OnLuaError(lua_State *L,
				std::exception_ptr e) noexcept override;
	};

	using NotifyRegistrationMap =
		std::map<std::string, NotifyRegistration, std::less<>>;
	NotifyRegistrationMap notify_registrations;

	/**
	 * A #SharedConnectionQuery implementation which sends LISTEN
	 * queries to PostgreSQL for all newly registered NOTIFY
	 * listeners, or sends LISTEN to all NOTIFY listeners if a new
	 * PostgreSQL connection is established.
	 */
	class ListenQuery final : public Pg::SharedConnectionQuery {
		NotifyRegistrationMap &notify_registrations;

	public:
		ListenQuery(Pg::SharedConnection &_shared_connection,
			    NotifyRegistrationMap &_notify_registrations) noexcept
			:Pg::SharedConnectionQuery(_shared_connection),
			 notify_registrations(_notify_registrations) {}

	private:
		/* virtual methods from class Pg::SharedConnectionQuery */
		void OnPgConnectionAvailable(Pg::AsyncConnection &c) {
			for (auto &[name, registration] : notify_registrations) {
				if (registration.registered)
					continue;

				const auto sql = fmt::format("LISTEN \"{}\"", name);
				c.Execute(sql.c_str());
				registration.registered = true;
			}

			Pg::SharedConnectionQuery::Cancel();
		}

		void OnPgError(std::exception_ptr e) noexcept {
			// TODO log?
			(void)e;
		}
	} listen_query{connection, notify_registrations};

public:
	template<typename... Args>
	explicit PgConnection(Args&&... args) noexcept
		:connection(std::forward<Args>(args)...,
			    static_cast<Pg::SharedConnectionHandler &>(*this)) {}

	auto &GetEventLoop() const noexcept {
		return connection.GetEventLoop();
	}

	int Execute(lua_State *L);
	int Listen(lua_State *L);

private:
	/* virtual methods from class Pg::SharedConnectionHandler */
	void OnPgConnect() override;
	void OnPgNotify(const char *name) override;
	void OnPgError(std::exception_ptr e) noexcept override;
};

static constexpr char lua_pg_connection_class[] = "pg.Connection";
using PgConnectionClass = Lua::Class<PgConnection, lua_pg_connection_class>;

static constexpr struct luaL_Reg lua_pg_connection_methods[] = {
	{"execute", PgConnectionClass::WrapMethod<&PgConnection::Execute>()},
	{"listen", PgConnectionClass::WrapMethod<&PgConnection::Listen>()},
	{nullptr, nullptr}
};

class PgRequest final
	: public Pg::SharedConnectionQuery, Pg::AsyncResultHandler
{
	lua_State *const L;

	DeferEvent defer_resume;

	Pg::Result result;

	std::exception_ptr error;

public:
	PgRequest(lua_State *_L,
		  Pg::SharedConnection &connection,
		  StackIndex sql, StackIndex params) noexcept
		:Pg::SharedConnectionQuery(connection),
		 L(_L),
		 defer_resume(connection.GetEventLoop(),
			      BIND_THIS_METHOD(OnDeferredResume))
	{
		const ScopeCheckStack check_stack(L);

		/* copy the parameters to fenv */
		lua_newtable(L);

		SetTable(L, RelativeStackIndex{-1}, "sql", sql);

		if (params.idx > 0)
			SetTable(L, RelativeStackIndex{-1}, "params", params);

		lua_setfenv(L, -2);
	}

	void SendQuery(Pg::AsyncConnection &c);

	void DeferResumeError(std::exception_ptr _error) noexcept {
		error = std::move(_error);
		defer_resume.Schedule();
	}

	void ResumeError(std::exception_ptr _error) noexcept {
		/* return [nil, error_message] for assert() */
		Push(L, nullptr);
		Push(L, _error);
		Resume(L, 2);
	}

private:
	void OnDeferredResume() noexcept {
		if (!result.IsDefined()) {
			if (error)
				ResumeError(std::move(error));
			else
				/* return nil */
				Resume(L, 0);
		} else if (result.IsError()) {
			/* return [nil, error_message] for assert() */
			Push(L, nullptr);
			Push(L, result.GetErrorMessage());
			Resume(L, 2);
		} else {
			/* return result object */
			NewPgResult(L, std::move(result));
			Resume(L, 1);
		}
	}

	/* virtual methods from Pg::SharedConnectionQuery */
	void OnPgConnectionAvailable(Pg::AsyncConnection &connection) override;
	void OnPgError(std::exception_ptr error) noexcept override;

	/* virtual methods from Pg::AsyncResultHandler */
	void OnResult(Pg::Result &&_result) override {
		result = std::move(_result);
	}

	void OnResultEnd() override {
		defer_resume.Schedule();

		Pg::SharedConnectionQuery::Cancel();
	}

	void OnResultError() noexcept override {
		defer_resume.Schedule();

		Pg::SharedConnectionQuery::Cancel();
	}
};

static constexpr char lua_pg_request_class[] = "pg.Request";
using PgRequestClass = Lua::Class<PgRequest, lua_pg_request_class>;

inline int
PgConnection::Execute(lua_State *L)
{
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Not enough parameters");
	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	luaL_checkstring(L, 2);
	constexpr StackIndex sql{2};

	StackIndex params{0};
	if (lua_gettop(L) >= 3) {
		luaL_checktype(L, 3, LUA_TTABLE);
		params = StackIndex{3};
	}

	auto *request = PgRequestClass::New(L, L, connection,
					    sql, params);
	connection.ScheduleQuery(*request);
	return lua_yield(L, 1);
}

inline int
PgConnection::Listen(lua_State *L)
{
	if (lua_gettop(L) < 3)
		return luaL_error(L, "Not enough parameters");
	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	constexpr int name_idx = 2;
	constexpr int handler_idx = 3;

	const char *name = luaL_checkstring(L, name_idx);
	luaL_checktype(L, 3, LUA_TFUNCTION);

	auto [_, inserted] = notify_registrations.try_emplace(name, L,
							      StackIndex{handler_idx});
	if (!inserted)
		luaL_argerror(L, name_idx, "Duplicate notify name");

	/* schedule a LISTEN to PostgreSQL */
	if (!listen_query.IsScheduled())
		connection.ScheduleQuery(listen_query);

	return 0;
}

void
PgConnection::OnPgConnect()
{
	if (!notify_registrations.empty()) {
		/* if a new PostgreSQL connection is established, we
		   need to re-run LISTEN for all NOTIFY listeners */
		for (auto &[_, registration] : notify_registrations)
			registration.registered = false;

		if (!listen_query.IsScheduled())
			connection.ScheduleQuery(listen_query);
	}
}

void
PgConnection::NotifyRegistration::Start() noexcept
{
	if (busy) {
		/* already running - do it again after this Lua
		   coroutine finishes */
		again = true;
		return;
	}

	auto *L = thread.CreateThread(*this);
	handler.Push(L);
	busy = true;

	Resume(L, 0);
}

void
PgConnection::NotifyRegistration::OnLuaFinished([[maybe_unused]] lua_State *L) noexcept
{
	assert(busy);
	busy = false;

	/* release the reference to the Lua thread */
	thread.Cancel();

	if (again) {
		again = false;
		Start();
	}
}

void
PgConnection::NotifyRegistration::OnLuaError([[maybe_unused]] lua_State *L,
					     std::exception_ptr e) noexcept
{
	assert(busy);
	busy = false;

	/* release the reference to the Lua thread */
	thread.Cancel();

	// TOOD log?
	(void)e;

	if (again) {
		again = false;
		Start();
	}
}

void
PgConnection::OnPgNotify(const char *name)
{
	const auto i = notify_registrations.find(name);
	if (i == notify_registrations.end())
		return;

	i->second.Start();
}

void
PgConnection::OnPgError(std::exception_ptr e) noexcept
{
	// TODO log?
	(void)e;
}

inline void
PgRequest::SendQuery(Pg::AsyncConnection &c)
{
	const ScopeCheckStack check_stack(L);

	/* stack[-2] = fenv.sql; stack[-1] = fenv.params */
	lua_getfenv(L, -1);
	lua_getfield(L, -1, "sql");
	lua_getfield(L, -2, "params");
	AtScopeExit(_L=L) { lua_pop(_L, 3); };

	if (!lua_isnil(L, -1)) {
		const std::size_t n = lua_objlen(L, -1);
		AllocatedArray<const char *> p(n);

		std::forward_list<StringBuffer<64>> number_buffers;

		for (std::size_t i = 0; i < n; ++i) {
			lua_rawgeti(L, -1, i + 1);
			AtScopeExit(_L=L) { lua_pop(_L, 1); };

			const auto type = lua_type(L, -1);
			switch (type) {
			case LUA_TNIL:
				p[i] = nullptr;
				break;

			case LUA_TBOOLEAN:
				p[i] = lua_toboolean(L, -1) ? "1" : "0";
				break;

			case LUA_TNUMBER:
				number_buffers.emplace_front();
				snprintf(number_buffers.front().data(),
					 number_buffers.front().capacity(),
					 "%g", (double)lua_tonumber(L, -1));
				p[i] = number_buffers.front().c_str();
				break;

			case LUA_TSTRING:
				p[i] = lua_tostring(L, -1);
				break;

			default:
				throw FmtRuntimeError("Unsupported query parameter type: {}",
						      lua_typename(L, type));
			}
		}

		c.SendQueryParams(*this, false, lua_tostring(L, -2),
				  n, p.data(),
				  nullptr, nullptr);
	} else {
		c.SendQuery(*this, lua_tostring(L, -2));
	}
}

void
PgRequest::OnPgConnectionAvailable(Pg::AsyncConnection &connection)
{
	SendQuery(connection);
}

void
PgRequest::OnPgError(std::exception_ptr _error) noexcept
{
	DeferResumeError(std::move(_error));
}

void
InitPgConnection(lua_State *L) noexcept
{
	PgConnectionClass::Register(L);
	luaL_newlib(L, lua_pg_connection_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	PgRequestClass::Register(L);

	SetField(L, RelativeStackIndex{-1}, "__close", [](auto _L){
		auto &request = PgRequestClass::Cast(_L, 1);
		request.Cancel();
		return 0;
	});

	lua_pop(L, 1);
}

void
NewPgConnection(struct lua_State *L, EventLoop &event_loop,
		const char *conninfo, const char *schema) noexcept
{
	PgConnectionClass::New(L, event_loop, conninfo, schema);
}

} // namespace Lua
