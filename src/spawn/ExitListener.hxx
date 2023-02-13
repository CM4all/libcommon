// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

/**
 * This interface gets notified when the registered child process
 * exits.
 */
class ExitListener {
public:
	virtual void OnChildProcessExit(int status) noexcept = 0;
};
