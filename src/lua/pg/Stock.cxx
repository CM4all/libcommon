/*
 * Copyright 2021 CM4all GmbH
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
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/Ref.hxx"
#include "lua/Resume.hxx"
#include "pg/AsyncConnection.hxx"
#include "pg/Stock.hxx"
#include "stock/GetHandler.hxx"
#include "event/DeferEvent.hxx"
#include "util/ScopeExit.hxx"

extern "C" {
#include <lauxlib.h>
}

namespace Lua {

class PgStock {
	Pg::Stock stock;

public:
	template<typename... Args>
	explicit PgStock(Args&&... args) noexcept
		:stock(std::forward<Args>(args)...) {}

private:
	static int Execute(lua_State *L);
	int Execute(lua_State *L, Ref &&sql);

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

	Ref sql;

	Pg::Result result;

public:
	PgRequest(lua_State *_L, Stock &stock, Ref &&_sql) noexcept
		:L(_L),
		 defer_resume(stock.GetEventLoop(),
			      BIND_THIS_METHOD(OnDeferredResume)),
		 sql(std::move(_sql))
	{
		stock.Get({}, *this, cancel_ptr);
	}

	~PgRequest() noexcept {
		if (cancel_ptr)
			cancel_ptr.Cancel();
		else if (item != nullptr) {
			auto &connection = Pg::Stock::GetConnection(*item);
			connection.DiscardRequest();
			item->Put(true);
		}
	}

private:
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

		sql.Push(L);
		AtScopeExit(L=L) { lua_pop(L, 1); };

		auto &connection = Pg::Stock::GetConnection(*item);
		connection.SendQuery(*this, lua_tostring(L, -1));
	}

	void OnStockItemError(std::exception_ptr error) noexcept override {
		cancel_ptr = nullptr;

		/* return [nil, error_message] for assert() */
		Push(L, nullptr);
		Push(L, error);
		Resume(L, 2);
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
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	luaL_checkstring(L, 2);
	Ref sql{L, StackIndex{2}};

	auto &stock = PgStockClass::Cast(L, 1);
	return stock.Execute(L, std::move(sql));
}

inline int
PgStock::Execute(lua_State *L, Ref &&sql)
{
	PgRequestClass::New(L, L, stock, std::move(sql));
	return lua_yield(L, 1);
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
