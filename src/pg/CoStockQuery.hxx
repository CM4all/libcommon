// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Stock.hxx"
#include "CoQuery.hxx"
#include "stock/CoGet.hxx"
#include "co/Task.hxx"
#include "util/ScopeExit.hxx"

namespace Pg {

/**
 * Obtain a PostgreSQL connection from a #Pg::Stock, send the given
 * query, return the connection to the stock and return the
 * #Pg::Result.
 */
template<typename... Params>
Co::EagerTask<Result>
CoStockQuery(Stock &stock, const char *query, const Params&... params) noexcept
{
	/* construct the ParamArray here (and return an EagerTask) to
	   avoid dangling references in the "params", just in case the
	   task gets passed around */
	const AutoParamArray<Params...> param_array(params...);

	auto *item = co_await CoStockGet(stock, {});
	AtScopeExit(item) { item->Put(PutAction::REUSE); };

	co_return co_await CoQuery(stock.GetConnection(*item), query,
				   param_array);
}

} // namespace Co
