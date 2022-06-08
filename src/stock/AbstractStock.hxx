/*
 * Copyright 2007-2022 CM4all GmbH
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

	virtual void Put(StockItem &item, bool destroy) noexcept = 0;
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
