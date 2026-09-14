// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

/**
 * Information about the spawer process and the kernel.
 */
struct SpawnContext {
	/**
	 * Do we have CAP_SYS_ADMIN?
	 */
	bool is_sys_admin;

	SpawnContext() noexcept;
};
