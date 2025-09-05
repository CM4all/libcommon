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
	bool ipc = false;
	bool pid = false;

	/**
	 * UID mapping for user namespace. If non-nullptr data(), a user
	 * namespace will be created and this string will be written to
	 * /proc/self/uid_map.
	 */
	std::string_view uid_map{};

	/**
	 * GID mapping for user namespace. If non-nullptr data(), a user
	 * namespace will be created and this string will be written to
	 * /proc/self/gid_map.
	 */
	std::string_view gid_map{};
};

struct NamespacesResponse {
	UniqueFileDescriptor ipc, pid;

	/**
	 * User namespace file descriptor. Set when a user namespace
	 * was created (when either uid_map or gid_map had non-nullptr data()).
	 */
	UniqueFileDescriptor user;
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
