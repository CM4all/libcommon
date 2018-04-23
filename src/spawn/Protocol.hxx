/*
 * Copyright 2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdint.h>

/*

  Definitions for the Spawn daemon protocol
  (https://github.com/CM4all/spawn).

  The Spawn daemon listens on a local seqpacket socket for commands.

  Each datagram begins with the 32 bit "magic", followed by a CRC32 of
  all command packets, followed by one or more command packets
  (currently, only one is allowed).  Each command packet begins with a
  header and a variable-length payload.  The payloads are padded to
  the next multiple of 4 bytes.

  All integers are native endian.  This protocol is designed for
  communication over local sockets (AF_LOCAL), and thus has no need
  for conversion to network byte order.

 */

namespace SpawnDaemon {

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
     * Create a new namespace with the specified name (or look up the
     * name in the table of existing managed namespaces).
     *
     * Payload is an "uint32_t" with one or more "CLONE_*" namespace
     * flags, followed by a non-empty variable-length name.
     *
     * Response may be #ResponseCommand::NAMESPACE_HANDLES or
     * #ResponseCommand::ERROR.
     */
    MAKE_NAMESPACES = 1,
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
     * Successful response to #RequestCommand::MAKE_NAMESPACES.
     *
     * Payload is a list of "uint32_t" values, each of them denoting a
     * single "CLONE_*" flag.  The defines the order in which the
     * namespace file handles are being transmitted as ancillary data.
     */
    NAMESPACE_HANDLES = 1,
};

struct ResponseHeader {
    uint16_t size;
    ResponseCommand command;
};

}
