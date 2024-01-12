// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Push.hxx"
#include "lua/Util.hxx"
#include "util/SpanCast.hxx"

#include <nlohmann/json.hpp>

static std::string_view
ToStringView(const std::vector<uint8_t> &src) noexcept
{
	return ToStringView(std::as_bytes(std::span{src.data(), src.size()}));
}

namespace Lua {

void
Push(lua_State *L, const nlohmann::json &j)
{
	switch (j.type()) {
	case nlohmann::json::value_t::null:
	case nlohmann::json::value_t::discarded:
		break;

	case nlohmann::json::value_t::boolean:
		Push(L, j.get<bool>());
		return;

	case nlohmann::json::value_t::number_float:
		Push(L, j.get<double>());
		return;

	case nlohmann::json::value_t::array:
		lua_newtable(L);

		for (std::size_t i = 0, n = j.size(); i != n; ++i) {
			Push(L, j[i]);
			lua_rawseti(L, -2, i + 1);
		}

		return;

	case nlohmann::json::value_t::object:
		lua_newtable(L);

		for (const auto &[key, value] : j.items())
			SetTable(L, RelativeStackIndex{-1},
				 static_cast<std::string_view>(key),
				 value);

		return;

	case nlohmann::json::value_t::number_integer:
	case nlohmann::json::value_t::number_unsigned:
		Push(L, j.get<lua_Integer>());
		return;

	case nlohmann::json::value_t::string:
		Push(L, j.get<std::string_view>());
		return;

	case nlohmann::json::value_t::binary:
		Push(L, ToStringView(j.get<nlohmann::json::binary_t>()));
		return;
	}

	lua_pushnil(L);
}

} // namespace Lua
