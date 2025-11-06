// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Socket.hxx"
#include "SocketAddress.hxx"
#include "lua/CheckArg.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/ForEach.hxx"
#include "lua/StringView.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/SpanCast.hxx"

extern "C" {
#include <lauxlib.h>
}

using std::string_view_literals::operator""sv;

namespace Lua {

[[nodiscard]]
static int
PushSocketError(lua_State *L, socket_error_t error=GetSocketError())
{
	Push(L, nullptr);
	Push(L, static_cast<const char *>(SocketErrorMessage{error}));
	return 2;
}

class Socket final {
	UniqueSocketDescriptor socket;

public:
	explicit Socket(UniqueSocketDescriptor &&_socket) noexcept
		:socket(std::move(_socket)) {}

	static const struct luaL_Reg methods[];

private:
	int Close(lua_State *L);
	int Send(lua_State *L);
};

inline int
Socket::Close(lua_State *L)
{
	const int top = lua_gettop(L);
	if (top > 1)
		return luaL_error(L, "Too many parameters");

	socket.Close();
	return 0;
}

inline int
Socket::Send(lua_State *L)
{
	const int top = lua_gettop(L);
	if (top < 2)
		return luaL_error(L, "Not enough parameters");
	else if (top > 4)
		return luaL_error(L, "Too many parameters");

	std::string_view src = CheckStringView(L, 2);

	if (top >= 4) {
		int i = luaL_checkinteger(L, 4);
		if (i < 0)
			i = static_cast<int>(src.size()) + 1 + i;
		if (i < 1 || static_cast<std::size_t>(i) > src.size())
			luaL_argerror(L, 4, "Bad string position");

		src = src.substr(0, i);
	}

	if (top >= 3) {
		int i = luaL_checkinteger(L, 3);
		if (i < 0)
			i = static_cast<int>(src.size()) + 1 + i;
		if (i < 1 || static_cast<std::size_t>(i) > src.size())
			luaL_argerror(L, 3, "Bad string position");

		src = src.substr(i - 1);
	}

	ssize_t nbytes = 0;
	if (!src.empty()) {
		nbytes = socket.Send(AsBytes(src));
		if (nbytes < 0)
			return PushSocketError(L);
	}

	Push(L, nbytes);
	return 1;
}

static constexpr char lua_socket_class[] = "socket";
using SocketClass = Class<Socket, lua_socket_class>;

constexpr struct luaL_Reg Socket::methods[] = {
	{"close", SocketClass::WrapMethod<&Socket::Close>()},
	{"send", SocketClass::WrapMethod<&Socket::Send>()},
	{nullptr, nullptr}
};

static int
NewConnectedSocket(lua_State *L)
{
	const int top = lua_gettop(L);
	if (top < 2)
		return luaL_error(L, "Not enough parameters");
	else if (top > 3)
		return luaL_error(L, "Too many parameters");

	const auto address = ToSocketAddress(L, 2, 0);

	int type = SOCK_STREAM;

	if (top >= 3) {
		ForEach(L, 3, [L, &type](auto key_idx, auto value_idx){
			if (lua_type(L, GetStackIndex(key_idx)) != LUA_TSTRING)
				return (void)luaL_error(L, "Key is not a string");

			const std::string_view key = ToStringView(L, GetStackIndex(key_idx));
			if (key == "type"sv) {
				if (!lua_isstring(L, GetStackIndex(value_idx)))
					return (void)luaL_error(L, "'type' must be a string");

				const std::string_view value = ToStringView(L, GetStackIndex(value_idx));
				if (value == "stream"sv)
					type = SOCK_STREAM;
				else if (value == "dgram"sv)
					type = SOCK_DGRAM;
				else if (value == "seqpacket"sv)
					type = SOCK_SEQPACKET;
				else
					return (void)luaL_error(L, "Unsupported socket type");
			} else
				return (void)luaL_error(L, "Unrecognized option");
		});
	}

	UniqueSocketDescriptor s;
	if (!s.Create(address.GetFamily(), type, 0))
		return PushSocketError(L);

	if (!s.Connect(address))
		return PushSocketError(L);

	SocketClass::New(L, std::move(s));
	return 1;
}

void
InitSocket(lua_State *L)
{
	SocketClass::Register(L);
	luaL_newlib(L, Socket::methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_newtable(L);
	SetTable(L, RelativeStackIndex{-1}, "connect", NewConnectedSocket);
	lua_setglobal(L, "socket");
}

} // namespace Lua
