// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "NetstringHeader.hxx"

#include <list>
#include <span>

class NetstringGenerator {
	NetstringHeader header;

public:
	/**
	 * @param comma generate the trailing comma?
	 */
	void operator()(std::list<std::span<const std::byte>> &list,
			bool comma=true) noexcept;
};
