/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SPAWN_INIT_HXX
#define SPAWN_INIT_HXX

#include <sys/types.h>

/**
 * Fork the "real" child process from this one, which will become
 * "init".
 *
 * Throws an exception on error.
 *
 * @return 0 if this is the child process, or the child's process id
 * if this is "init"
 */
pid_t
SpawnInitFork();

/**
 * An "init" implementation for PID namespaces.
 *
 * @param child_pid the main child's process id obtained from
 * SpawnInitFork()
 * @return the exit status
 */
int
SpawnInit(pid_t child_pid);

#endif
