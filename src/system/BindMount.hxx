// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

/**
 * Throws std::system_error on error.
 */
void
BindMount(const char *source, const char *target, int flags);
