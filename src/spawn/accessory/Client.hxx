// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class UniqueFileDescriptor;
class SocketDescriptor;
class UniqueSocketDescriptor;

namespace SpawnAccessory {

/**
 * Connect to the local Spawn daemon.
 */
UniqueSocketDescriptor
Connect();

/**
 * Ask the Spawn daemon to create a new PID namespace.
 *
 * Throws on error.
 *
 * @return the namespace descriptor
 */
UniqueFileDescriptor
MakePidNamespace(SocketDescriptor s, const char *name);

}
