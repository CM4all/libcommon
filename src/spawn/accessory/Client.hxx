// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

#include <string_view>

class UniqueFileDescriptor;
class SocketDescriptor;
class UniqueSocketDescriptor;

namespace SpawnAccessory {

/**
 * Connect to the local Spawn daemon.
 */
UniqueSocketDescriptor
Connect();

struct NamespacesRequest {
	bool pid = false;
};

struct NamespacesResponse {
	UniqueFileDescriptor pid;
};

/**
 * Ask the Spawn daemon to create namespaces.
 *
 * Throws on error.
 *
 * @return a struct containing namespace descriptors
 */
NamespacesResponse
MakeNamespaces(SocketDescriptor s, std::string_view name,
	       NamespacesRequest request);

}
