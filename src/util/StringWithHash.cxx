// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "StringWithHash.hxx"
#include "djb_hash.hxx"
#include "SpanCast.hxx"

StringWithHash::StringWithHash(std::string_view _value) noexcept
	:hash(djb_hash(AsBytes(_value))), value(_value) {}
