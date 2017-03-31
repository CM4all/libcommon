/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef BIND_MOUNT_HXX
#define BIND_MOUNT_HXX

bool
BindMount(const char *source, const char *target, int flags);

#endif
