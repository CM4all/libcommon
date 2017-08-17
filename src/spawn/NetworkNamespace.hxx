/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

/**
 * Reassociate the current process with the given network namespace
 * (set up with "ip netns" mounted in /run/netns/).
 */
void
ReassociateNetworkNamespace(const char *name);
