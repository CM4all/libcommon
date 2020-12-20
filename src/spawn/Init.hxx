/*
 * Copyright 2007-2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
