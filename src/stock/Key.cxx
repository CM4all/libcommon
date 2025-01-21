// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Key.hxx"
#include "util/djb_hash.hxx"
#include "util/SpanCast.hxx"

StockKey::StockKey(std::string_view _value) noexcept
	:hash(djb_hash(AsBytes(_value))), value(_value) {}
