/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "UserNamespace.hxx"
#include "system/Error.hxx"
#include "io/WriteFile.hxx"

#include <assert.h>
#include <string.h>
#include <stdio.h>

void
DenySetGroups()
{
    TryWriteExistingFile("/proc/self/setgroups", "deny");
}

static void
WriteFileOrThrow(const char *path, const char *data)
{
    if (TryWriteExistingFile(path, data) == WriteFileResult::ERROR)
        throw FormatErrno("write('%s') failed", path);
}

void
SetupUidMap(unsigned pid, unsigned uid, bool root)
{
    char path_buffer[64], data_buffer[256];

    const char *path = "/proc/self/uid_map";
    if (pid > 0) {
        sprintf(path_buffer, "/proc/%u/uid_map", pid);
        path = path_buffer;
    }

    size_t position = sprintf(data_buffer, "%u %u 1\n", uid, uid);
    if (root && uid != 0)
        strcpy(data_buffer + position, "0 0 1\n");

    WriteFileOrThrow(path, data_buffer);
}

void
SetupGidMap(unsigned pid, unsigned gid, bool root)
{
    char path_buffer[64], data_buffer[256];

    const char *path = "/proc/self/gid_map";
    if (pid > 0) {
        sprintf(path_buffer, "/proc/%u/gid_map", pid);
        path = path_buffer;
    }

    size_t position = sprintf(data_buffer, "%u %u 1\n", gid, gid);
    if (root && gid != 0)
        strcpy(data_buffer + position, "0 0 1\n");

    WriteFileOrThrow(path, data_buffer);
}

void
SetupGidMap(unsigned pid, const std::set<unsigned> &gids)
{
    assert(!gids.empty());

    char path_buffer[64], data_buffer[1024];

    const char *path = "/proc/self/gid_map";
    if (pid > 0) {
        sprintf(path_buffer, "/proc/%u/gid_map", pid);
        path = path_buffer;
    }

    size_t position = 0;
    for (unsigned i : gids) {
        if (position + 64 > sizeof(data_buffer))
            break;

        position += sprintf(data_buffer + position, "%u %u 1\n", i, i);
    }

    WriteFileOrThrow(path, data_buffer);
}
