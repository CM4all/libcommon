// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

/**
 * Reassociate the next child process with the given PID namespace.
 * The namespace handle is queried from the Spawn daemon (Package
 * "cm4all-spawn").
 *
 * Throws on error.
 */
void
ReassociatePidNamespace(const char *name);
