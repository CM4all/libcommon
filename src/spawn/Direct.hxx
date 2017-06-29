/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SPAWN_DIRECT_HXX
#define SPAWN_DIRECT_HXX

#include <sys/types.h>

struct PreparedChildProcess;
struct CgroupState;

/**
 * Throws exception on error.
 *
 * @return the process id
 */
pid_t
SpawnChildProcess(PreparedChildProcess &&params,
                  const CgroupState &cgroup_state);

#endif
