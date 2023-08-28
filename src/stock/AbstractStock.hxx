// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "PutAction.hxx"

#include <exception>

class StockItem;
class StockGetHandler;
class EventLoop;

/**
 * Abstract base class for #Stock which allows other containers to
 * manage #StockItem instances.
 */
class AbstractStock {
public:
	[[gnu::const]]
	virtual const char *GetName() const noexcept = 0;

	[[gnu::const]]
	virtual EventLoop &GetEventLoop() const noexcept = 0;

	virtual void Put(StockItem &item, PutAction action) noexcept = 0;
	virtual void ItemIdleDisconnect(StockItem &item) noexcept = 0;
	virtual void ItemBusyDisconnect(StockItem &item) noexcept = 0;
	virtual void ItemCreateSuccess(StockGetHandler &get_handler,
				       StockItem &item) noexcept = 0;
	void ItemCreateError(StockItem &item, StockGetHandler &get_handler,
			     std::exception_ptr ep) noexcept;
	virtual void ItemCreateError(StockGetHandler &get_handler,
				     std::exception_ptr ep) noexcept = 0;
	void ItemCreateAborted(StockItem &item) noexcept;
	virtual void ItemCreateAborted() noexcept = 0;
	virtual void ItemUncleanFlagCleared() noexcept {}
};
