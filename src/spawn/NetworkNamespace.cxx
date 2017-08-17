/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "NetworkNamespace.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <sched.h>

/**
 * Open a network namespace in /run/netns.
 */
static UniqueFileDescriptor
OpenNetworkNS(const char *name)
{
    char path[4096];
    if (snprintf(path, sizeof(path),
                 "/run/netns/%s", name) >= (int)sizeof(path))
        throw std::runtime_error("Network namespace name is too long");

    UniqueFileDescriptor fd;
    if (!fd.OpenReadOnly(path))
        throw FormatErrno("Failed to open %s", path);

    return fd;
}

void
ReassociateNetworkNamespace(const char *name)
{
    assert(name != nullptr);

    if (setns(OpenNetworkNS(name).Get(), CLONE_NEWNET) < 0)
        throw FormatErrno("Failed to reassociate with network namespace '%s'",
                          name);
}
