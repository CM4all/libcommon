// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstdint>

/*

  Definitions for the Spawn Accessory daemon protocol
  (https://github.com/CM4all/spawn).

  The Spawn Accessory daemon listens on a local seqpacket socket for
  commands.

  Each datagram begins with the 32 bit "magic", followed by a CRC32 of
  all command packets, followed by one or more command packets.

  Each command packet begins with a header and a variable-length
  payload.  The payloads are padded to the next multiple of 4 bytes.

  These command packets belong together; they construct a larger
  request; for example, the first command may specify the namespace
  name, and the following packets specify the types of namespaces.

  All integers are native endian.  This protocol is designed for
  communication over local sockets (AF_LOCAL), and thus has no need
  for conversion to network byte order.

*/

namespace SpawnAccessory {

/**
 * This magic number precedes every datagram.
 */
static const uint32_t MAGIC = 0x63046173;

struct DatagramHeader {
	uint32_t magic;
	uint32_t crc;
};

enum class RequestCommand : uint16_t {
	NOP = 0,

	/**
	 * Set the name of namespaces requested by this datagram.  Payload
	 * is a non-empty variable-length name (7 bit ASCII, no null
	 * bytes).
	 */
	NAME = 1,

	/**
	 * Create a new IPC namespace.  No payload.
	 *
	 * Response may be #ResponseCommand::NAMESPACE_HANDLES or
	 * #ResponseCommand::ERROR.
	 */
	IPC_NAMESPACE = 2,

	/**
	 * Create a new PID namespace.  No payload.
	 *
	 * Response may be #ResponseCommand::NAMESPACE_HANDLES or
	 * #ResponseCommand::ERROR.
	 */
	PID_NAMESPACE = 3,

	/**
	 * Create a new user namespace.  Optional payload: if non-empty,
	 * consists of two strings separated by a null byte; the first
	 * one is the uid_map to be written to /proc/self/uid_map; the
	 * second one is the gid_map for /proc/self/gid_map.
	 *
	 * Response may be #ResponseCommand::NAMESPACE_HANDLES or
	 * #ResponseCommand::ERROR.
	 */
	USER_NAMESPACE = 4,

	/**
	 * Create a lease pipe.  The namespaces created with this
	 * datagram will be kept alive at least until the client
	 * closes the pipe returned by this command.
	 *
	 * No payload.
	 *
	 * Response may be #ResponseCommand::LEASE_PIPE or
	 * #ResponseCommand::ERROR.
	 */
	LEASE_PIPE = 5,
};

struct RequestHeader {
	uint16_t size;
	RequestCommand command;
};

enum class ResponseCommand : uint16_t {
	/**
	 * The request has failed.
	 *
	 * Payload is a human-readable error message.
	 */
	ERROR = 0,

	/**
	 * Successful response to #RequestCommand::*_NAMESPACE.
	 *
	 * Payload is a list of "uint32_t" values, each of them denoting a
	 * single "CLONE_*" flag.  This defines the order in which the
	 * namespace file handles are being transmitted as ancillary data.
	 */
	NAMESPACE_HANDLES = 1,

	/**
	 * Successful response to #RequestCommand::LEASE_PIPE.
	 *
	 * No payload. The write side of the lease pipe is transmitted
	 * as ancillary data.
	 */
	LEASE_PIPE = 2,
};

struct ResponseHeader {
	uint16_t size;
	ResponseCommand command;
};

}
