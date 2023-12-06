// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "CoRunner.hxx"
#include "Resume.hxx"

namespace Lua {

/**
 * A helper class that invokes the global function "reload".  This may
 * be used by applications to delegate a SIGHUP signal to Lua code.
 */
class ReloadRunner final : ResumeListener {
	CoRunner runner;

	enum class State {
		IDLE,
		BUSY,
		AGAIN,
	} state = State::IDLE;

public:
	explicit ReloadRunner(lua_State *L) noexcept
		:runner(L) {}

	void Start() noexcept;

private:
	/* virtual methods from ResumeListener */
	void OnLuaFinished(lua_State *L) noexcept override;
	void OnLuaError(lua_State *L, std::exception_ptr e) noexcept override;
};

} // namespace Lua
