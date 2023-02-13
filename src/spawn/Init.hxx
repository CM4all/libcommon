// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sys/types.h>

/**
 * Fork the "real" child process from this one, which will become
 * "init".
 *
 * Throws an exception on error.
 *
 * @param name a name for this init process; it appears on the its name
 * @return 0 if this is the child process, or the child's process id
 * if this is "init"
 */
pid_t
SpawnInitFork(const char *name=nullptr);

/**
 * An "init" implementation for PID namespaces.
 *
 * Throws if an error occurs during initialization.
 *
 * @param child_pid the main child's process id obtained from
 * SpawnInitFork(); its exit status is returned by this function; a
 * non-positive value disables this feature
 * @param remain keep running after the last child process has exited?
 * In this mode, the function will return only after receiving
 * SIGTERM, SIGINT or SIGQUIT
 * @return the exit status
 */
int
SpawnInit(pid_t child_pid, bool remain);

/**
 * Fork an init process in a new PID namespace.
 *
 * Note: a side effect of this function is that the caller's
 * "pid_for_children" namespace is changed to the new PID namespace.
 *
 * Throws on error.
 *
 * @return pid the pid of the new init process (as seen by the
 * caller's PID namespace)
 */
pid_t
UnshareForkSpawnInit();
