// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>

class StockItem;

class StockGetHandler {
public:
	virtual void OnStockItemReady(StockItem &item) noexcept = 0;
	virtual void OnStockItemError(std::exception_ptr ep) noexcept = 0;
};
