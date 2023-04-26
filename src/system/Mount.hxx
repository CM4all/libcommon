// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

void
MountOrThrow(const char *source, const char *target,
	     const char *filesystemtype, unsigned long mountflags,
	     const void *data);

/**
 * Throws std::system_error on error.
 */
void
BindMount(const char *source, const char *target, unsigned long flags);
