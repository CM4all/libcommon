// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Panic.hxx"
#include "Util.hxx"

Lua::ScopePanicHandler::ScopePanicHandler(lua_State *_L)
	:L(_L), old(lua_atpanic(L, PanicHandler))
{
	SetRegistry(L, "ScopePanicHandler", LightUserData(this));
}

Lua::ScopePanicHandler::~ScopePanicHandler()
{
	SetRegistry(L, "ScopePanicHandler", nullptr);
	lua_atpanic(L, old);
}

int
Lua::ScopePanicHandler::PanicHandler(lua_State *L)
{
	auto &sph = *(ScopePanicHandler *)
		GetRegistryLightUserData(L, "ScopePanicHandler");
	longjmp(sph.j, 1);
}
