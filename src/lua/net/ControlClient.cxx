// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ControlClient.hxx"
#include "SocketAddress.hxx"
#include "net/control/Builder.hxx"
#include "net/control/Client.hxx"
#include "net/control/Protocol.hxx"
#include "net/ConnectSocket.hxx"
#include "net/SocketAddress.hxx"
#include "lua/CheckArg.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/StringView.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <fmt/core.h>

using std::string_view_literals::operator""sv;

namespace Lua {

class ControlBuilder final : public BengControl::Builder {
public:
	static const struct luaL_Reg methods[];

private:
	int AddSimple(lua_State *L, BengControl::Command command);

	int FadeChildren(lua_State *L) {
		return AddSimple(L, BengControl::Command::FADE_CHILDREN);
	}

	int FlushFilterCache(lua_State *L) {
		return AddSimple(L, BengControl::Command::FLUSH_FILTER_CACHE);
	}

	int DiscardSession(lua_State *L) {
		return AddSimple(L, BengControl::Command::DISCARD_SESSION);
	}

	int FlushHttpCache(lua_State *L) {
		return AddSimple(L, BengControl::Command::FLUSH_HTTP_CACHE);
	}

	int TerminateChildren(lua_State *L) {
		return AddSimple(L, BengControl::Command::TERMINATE_CHILDREN);
	}

	int DisconnectDatabase(lua_State *L) {
		return AddSimple(L, BengControl::Command::DISCONNECT_DATABASE);
	}

	int ResetLimiter(lua_State *L) {
		return AddSimple(L, BengControl::Command::RESET_LIMITER);
	}

	int RejectClient(lua_State *L) {
		return AddSimple(L, BengControl::Command::REJECT_CLIENT);
	}

	int TarpitClient(lua_State *L) {
		return AddSimple(L, BengControl::Command::TARPIT_CLIENT);
	}

	int CancelJob(lua_State *L);
};

int
ControlBuilder::AddSimple(lua_State *L, BengControl::Command command)
{
	const int top = lua_gettop(L);
	if (top < 2)
		return luaL_error(L, "Not enough parameters");

	for (int i = 2; i <= top; ++i)
		Add(command, ToStringView(L, i));

	// return self
	lua_settop(L, 1);
	return 1;
}

inline int
ControlBuilder::CancelJob(lua_State *L)
{
	const int top = lua_gettop(L);
	if (top < 3)
		return luaL_error(L, "Not enough parameters");
	if (top > 3)
		return luaL_error(L, "Too many parameters");

	const std::string_view partition_name = CheckStringView(L, 2);
	const std::string_view job_id = CheckStringView(L, 3);

	Add(BengControl::Command::CANCEL_JOB, fmt::format("{}\0{}"sv, partition_name, job_id));

	// return self
	lua_settop(L, 1);
	return 1;
}

static constexpr char lua_control_builder_class[] = "control_builder";
using ControlBuilderClass = Class<ControlBuilder, lua_control_builder_class>;

constexpr struct luaL_Reg ControlBuilder::methods[] = {
	{"fade_children", ControlBuilderClass::WrapMethod<&ControlBuilder::FadeChildren>()},
	{"flush_filter_cache", ControlBuilderClass::WrapMethod<&ControlBuilder::FlushFilterCache>()},
	{"discard_session", ControlBuilderClass::WrapMethod<&ControlBuilder::DiscardSession>()},
	{"flush_http_cache", ControlBuilderClass::WrapMethod<&ControlBuilder::FlushHttpCache>()},
	{"terminate_children", ControlBuilderClass::WrapMethod<&ControlBuilder::TerminateChildren>()},
	{"disconnect_database", ControlBuilderClass::WrapMethod<&ControlBuilder::DisconnectDatabase>()},
	{"reset_limiter", ControlBuilderClass::WrapMethod<&ControlBuilder::ResetLimiter>()},
	{"reject_client", ControlBuilderClass::WrapMethod<&ControlBuilder::RejectClient>()},
	{"tarpit_client", ControlBuilderClass::WrapMethod<&ControlBuilder::TarpitClient>()},
	{"cancel_job", ControlBuilderClass::WrapMethod<&ControlBuilder::CancelJob>()},
	{nullptr, nullptr}
};

class ControlClient final {
	const BengControl::Client client;

public:
	explicit ControlClient(BengControl::Client &&_client) noexcept
		:client(std::move(_client)) {}

	static const struct luaL_Reg methods[];

private:
	int Build(lua_State *L);
	int Send(lua_State *L);
};

inline int
ControlClient::Build(lua_State *L)
{
	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	ControlBuilderClass::New(L);
	return 1;
}

inline int
ControlClient::Send(lua_State *L)
try {
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Not enough parameters");

	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	client.Send(ControlBuilderClass::Cast(L, 2));
	return 0;
} catch (...) {
	// return [nil, error_message] for assert()
	Push(L, nullptr);
	Push(L, std::current_exception());
	return 2;
}

static constexpr char lua_control_client_class[] = "control_client";
using ControlClientClass = Class<ControlClient, lua_control_client_class>;

constexpr struct luaL_Reg ControlClient::methods[] = {
	{"build", ControlClientClass::WrapMethod<&ControlClient::Build>()},
	{"send", ControlClientClass::WrapMethod<&ControlClient::Send>()},
	{nullptr, nullptr}
};

static int
NewControlClient(lua_State *L)
try {
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Not enough parameters");

	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	if (lua_isstring(L, 2)) {
		ControlClientClass::New(L, BengControl::Client{lua_tostring(L, 2)});
		return 1;
	} else {
		ControlClientClass::New(L, BengControl::Client{CreateConnectDatagramSocket(GetSocketAddress(L, 2))});
		return 1;
	}
} catch (...) {
	// return [nil, error_message] for assert()
	Push(L, nullptr);
	Push(L, std::current_exception());
	return 2;
}

void
InitControlClient(lua_State *L)
{
	ControlBuilderClass::Register(L);
	luaL_newlib(L, ControlBuilder::methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	ControlClientClass::Register(L);
	luaL_newlib(L, ControlClient::methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_newtable(L);
	SetTable(L, RelativeStackIndex{-1}, "new", NewControlClient);
	lua_setglobal(L, "control_client");
}

} // namespace Lua
