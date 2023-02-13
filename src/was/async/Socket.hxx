// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <utility>

struct WasSocket {
	UniqueSocketDescriptor control;
	UniqueFileDescriptor input, output;

	void Close() noexcept {
		control.Close();
		input.Close();
		output.Close();
	}

	/**
	 * Throws on error.
	 */
	static std::pair<WasSocket, WasSocket> CreatePair();
};
