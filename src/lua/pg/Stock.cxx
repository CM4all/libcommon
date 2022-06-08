/*
 * Copyright 2021-2022 CM4all GmbH
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

#include "Stock.hxx"
#include "Result.hxx"
#include "lua/Assert.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/Resume.hxx"
#include "pg/AsyncConnection.hxx"
#include "pg/Stock.hxx"
#include "stock/GetHandler.hxx"
#include "event/DeferEvent.hxx"
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

class PgStock {
	Pg::Stock stock;

public:
	template<typename... Args>
	explicit PgStock(Args&&... args) noexcept
		:stock(std::forward<Args>(args)...) {}

private:
	static int Execute(lua_State *L);
	int Execute(lua_State *L, int sql, int params);

public:
	static constexpr struct luaL_Reg methods [] = {
		{"execute", Execute},
		{nullptr, nullptr}
	};
};

static constexpr char lua_pg_stock_class[] = "pg.Stock";
using PgStockClass = Lua::Class<PgStock, lua_pg_stock_class>;

class PgRequest final : StockGetHandler, Pg::AsyncResultHandler {
	lua_State *const L;

	DeferEvent defer_resume;

	CancellablePointer cancel_ptr;

	StockItem *item = nullptr;

	Pg::Result result;

public:
	PgRequest(lua_State *_L, Stock &stock,
		  int sql, int params) noexcept
		:L(_L),
		 defer_resume(stock.GetEventLoop(),
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

		/* start the operation */
		stock.Get({}, *this, cancel_ptr);
	}

	~PgRequest() noexcept {
		if (cancel_ptr)
			cancel_ptr.Cancel();
		else if (item != nullptr) {
			auto &connection = Pg::Stock::GetConnection(*item);
			connection.DiscardRequest();
			item->Put(false);
		}
	}

private:
	void ResumeError(std::exception_ptr error) noexcept {
		/* return [nil, error_message] for assert() */
		Push(L, nullptr);
		Push(L, error);
		Resume(L, 2);
	}

	void SendQuery(Pg::AsyncConnection &connection);

	void OnDeferredResume() noexcept {
		item->Put(true);
		item = nullptr;

		if (!result.IsDefined()) {
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

	/* virtual methods from StockGetHandler */
	void OnStockItemReady(StockItem &_item) noexcept override {
		cancel_ptr = nullptr;
		item = &_item;

		try {
			SendQuery(Pg::Stock::GetConnection(*item));
		} catch (...) {
			item = nullptr;
			_item.Put(true);

			ResumeError(std::current_exception());
		}
	}

	void OnStockItemError(std::exception_ptr error) noexcept override {
		cancel_ptr = nullptr;

		ResumeError(std::move(error));
	}

	/* virtual methods from Pg::AsyncResultHandler */
	void OnResult(Pg::Result &&_result) override {
		result = std::move(_result);
	}

	void OnResultEnd() override {
		defer_resume.Schedule();
	}

	void OnResultError() noexcept override {
		defer_resume.Schedule();
	}
};

static constexpr char lua_pg_request_class[] = "pg.Request";
using PgRequestClass = Lua::Class<PgRequest, lua_pg_request_class>;

int
PgStock::Execute(lua_State *L)
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

	auto &stock = PgStockClass::Cast(L, 1);
	return stock.Execute(L, sql, params);
}

inline int
PgStock::Execute(lua_State *L, int sql, int params)
{
	PgRequestClass::New(L, L, stock, sql, params);
	return lua_yield(L, 1);
}

inline void
PgRequest::SendQuery(Pg::AsyncConnection &connection)
{
	const ScopeCheckStack check_stack(L);

	/* stack[-2] = fenv.sql; stack[-1] = fenv.params */
	lua_getfenv(L, -1);
	lua_getfield(L, -1, "sql");
	lua_getfield(L, -2, "params");
	AtScopeExit(L=L) { lua_pop(L, 3); };

	if (!lua_isnil(L, -1)) {
		const std::size_t n = lua_objlen(L, -1);
		AllocatedArray<const char *> p(n);

		std::forward_list<StringBuffer<64>> number_buffers;

		for (std::size_t i = 0; i < n; ++i) {
			lua_rawgeti(L, -1, i + 1);
			AtScopeExit(L=L) { lua_pop(L, 1); };

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

		connection.SendQueryParams(*this, false, lua_tostring(L, -2),
					   n, p.data(),
					   nullptr, nullptr);
	} else {
		connection.SendQuery(*this, lua_tostring(L, -2));
	}
}

void
InitPgStock(lua_State *L) noexcept
{
	PgStockClass::Register(L);
	luaL_newlib(L, PgStock::methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	PgRequestClass::Register(L);
	lua_pop(L, 1);
}

void
NewPgStock(struct lua_State *L, EventLoop &event_loop,
	   const char *conninfo, const char *schema,
	   unsigned limit, unsigned max_idle) noexcept
{
	PgStockClass::New(L, event_loop, conninfo, schema,
			  limit, max_idle);
}

} // namespace Lua
