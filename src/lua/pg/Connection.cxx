// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Result.hxx"
#include "lua/Assert.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/Resume.hxx"
#include "pg/SharedConnection.hxx"
#include "util/AllocatedArray.hxx"
#include "util/Cancellable.hxx"
#include "util/RuntimeError.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringBuffer.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <forward_list>

#include <stdio.h>

namespace Lua {

class PgRequest;

class PgConnection final : Pg::SharedConnectionHandler {
	Pg::SharedConnection connection;

public:
	template<typename... Args>
	explicit PgConnection(Args&&... args) noexcept
		:connection(std::forward<Args>(args)...,
			    static_cast<Pg::SharedConnectionHandler &>(*this)) {}

	auto &GetEventLoop() const noexcept {
		return connection.GetEventLoop();
	}

private:
	static int Execute(lua_State *L);
	int Execute(lua_State *L, int sql, int params);

	/* virtual methods from class Pg::SharedConnectionHandler */
	void OnPgError(std::exception_ptr e) noexcept override;

public:
	static constexpr struct luaL_Reg methods [] = {
		{"execute", Execute},
		{nullptr, nullptr}
	};
};

static constexpr char lua_pg_connection_class[] = "pg.Connection";
using PgConnectionClass = Lua::Class<PgConnection, lua_pg_connection_class>;

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
		  int sql, int params) noexcept
		:Pg::SharedConnectionQuery(connection),
		 L(_L),
		 defer_resume(connection.GetEventLoop(),
			      BIND_THIS_METHOD(OnDeferredResume))
	{
		const ScopeCheckStack check_stack(L);

		/* copy the parameters to fenv */
		lua_newtable(L);

		lua_pushvalue(L, sql);
		lua_setfield(L, -2, "sql");

		if (params > 0) {
			lua_pushvalue(L, params);
			lua_setfield(L, -2, "params");
		}

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
PgConnection::Execute(lua_State *L, int sql, int params)
{
	auto *request = PgRequestClass::New(L, L, connection,
					    sql, params);
	connection.ScheduleQuery(*request);
	return lua_yield(L, 1);
}

int
PgConnection::Execute(lua_State *L)
{
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Not enough parameters");
	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	luaL_checkstring(L, 2);
	int sql = 2;

	int params = 0;
	if (lua_gettop(L) >= 3) {
		luaL_checktype(L, 3, LUA_TTABLE);
		params = 3;
	}

	auto &connection = PgConnectionClass::Cast(L, 1);
	return connection.Execute(L, sql, params);
}

inline void
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
				throw FormatRuntimeError("Unsupported query parameter type: %s",
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
	luaL_newlib(L, PgConnection::methods);
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
