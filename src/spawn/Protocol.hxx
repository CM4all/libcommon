/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef BENG_PROXY_SPAWN_PROTOCOL_HXX
#define BENG_PROXY_SPAWN_PROTOCOL_HXX

#include <stdint.h>

enum class SpawnRequestCommand : uint8_t {
    CONNECT,
    EXEC,
    KILL,
};

enum class SpawnExecCommand : uint8_t {
    ARG,
    SETENV,
    UMASK,
    STDIN,
    STDOUT,
    STDERR,
    STDERR_PATH,
    CONTROL,
    TTY,
    REFENCE,
    USER_NS,
    PID_NS,
    NETWORK_NS,
    IPC_NS,
    MOUNT_NS,
    MOUNT_PROC,
    PIVOT_ROOT,
    MOUNT_HOME,
    MOUNT_TMP_TMPFS,
    MOUNT_TMPFS,
    BIND_MOUNT,
    HOSTNAME,
    RLIMIT,
    UID_GID,
    FORBID_USER_NS,
    NO_NEW_PRIVS,
    CGROUP,
    CGROUP_SET,
    PRIORITY,
    SCHED_IDLE_,
    IOPRIO_IDLE,
    CHROOT,
    CHDIR,
    HOOK_INFO,
};

enum class SpawnResponseCommand : uint16_t {
    /**
     * Indicates that cgroups are available.
     */
    CGROUPS_AVAILABLE,

    EXIT,
};

#endif
