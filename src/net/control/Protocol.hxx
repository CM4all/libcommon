// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Definitions for the beng-proxy remote control protocol.
 */

#pragma once

#include <cstdint>

namespace BengControl {

/**
 * The default port for the control protocol.
 */
static constexpr unsigned DEFAULT_PORT = 5478;

enum class Command {
    NOP = 0,

    /**
     * Drop items from the translation cache.
     */
    TCACHE_INVALIDATE = 1,

    /**
     * Re-enable the specified node after a failure, remove all
     * failure/fade states.
     *
     * The payload is the node name according to lb.conf, followed by
     * a colon and the port number.
     */
    ENABLE_NODE = 2,

    /**
     * Fade out the specified node, preparing for its shutdown: the
     * node will only be used for pre-existing sessions that refer
     * to it.
     *
     * The payload is the node name according to lb.conf, followed by
     * a colon and the port number.
     */
    FADE_NODE = 3,

    /**
     * Get the status of the specified node.
     *
     * The payload is the node name according to lb.conf, followed by
     * a colon and the port number.
     *
     * The server then sends a response to the source IP.  Its payload
     * is the node name and port, a null byte, and a string describing
     * the worker status.  Possible values: "ok", "error", "fade".
     */
    NODE_STATUS = 4,

    /**
     * Dump all memory pools.
     */
    DUMP_POOLS = 5,

    /**
     * Server statistics.
     */
    STATS = 6,

    /**
     * Set the logger verbosity.  The payload is one byte: 0 means
     * quiet, 1 is the default, and bigger values make the daemon more
     * verbose.
     */
    VERBOSE = 7,

    /**
     * Fade out all child processes (FastCGI, WAS, LHTTP, Delegate;
     * but not beng-proxy workers).  These will not be used for new
     * requests; instead, fresh child processes will be launched.
     * Idle child processes will be killed immediately, and the
     * remaining ones will be killed as soon as their current work is
     * done.
     *
     * If a payload is given, then this is a tag which fades only
     * child processes with the given CHILD_TAG.
     */
    FADE_CHILDREN = 8,

    /**
     * Unregister all Zeroconf services.
     */
    DISABLE_ZEROCONF = 9,

    /**
     * Re-register all Zeroconf services.
     */
    ENABLE_ZEROCONF = 10,

    /**
     * Flush the NFS cache.
     */
    FLUSH_NFS_CACHE = 11,

    /**
     * Drop items from the filter cache.
     *
     * If a payload is given, then only cache items with the specified
     * tag will be flushed.
     */
    FLUSH_FILTER_CACHE = 12,

    /**
     * Write stopwatch data in human-readable text format into the
     * given pipe.
     */
    STOPWATCH_PIPE = 13,

    /**
     * Discard the session with the given
     * #TranslationCommand::ATTACH_SESSION value.
     */
    DISCARD_SESSION = 14,

    /**
     * Drop items from the HTTP cache with the given tag.
     */
    FLUSH_HTTP_CACHE = 15,

    /**
     * Terminate all child processes with the CHILD_TAG from the
     * payload.  Unlike #FADE_CHILDREN, this does not wait for
     * completion of the child's currently work.
     */
    TERMINATE_CHILDREN = 16,

    /**
     * Disable all queues, i.e. do not accept any new jobs.  If the
     * payload is not empty, then it is the name of the queue
     * (partition) which shall be disabled.
     *
     * Used by Workshop.
     */
    DISABLE_QUEUE = 17,

    /**
     * Re-enable all queues, i.e. resume accepting new jobs.  If the
     * payload is not empty, then it is the name of the queue
     * (partition) which shall be enabled.
     *
     * Used by Workshop.
     */
    ENABLE_QUEUE = 18,
};

struct ControlStats {
    /**
     * Number of open incoming connections.
     */
    uint32_t incoming_connections;

    /**
     * Number of open outgoing connections.
     */
    uint32_t outgoing_connections;

    /**
     * Number of child processes.
     */
    uint32_t children;

    /**
     * Number of sessions.
     */
    uint32_t sessions;

    /**
     * Total number of incoming HTTP requests that were received since
     * the server was started.
     */
    uint64_t http_requests;

    /**
     * The total allocated size of the translation cache in the
     * server's memory [bytes].
     */
    uint64_t translation_cache_size;

    /**
     * The total allocated size of the HTTP cache in the server's
     * memory [bytes].
     */
    uint64_t http_cache_size;

    /**
     * The total allocated size of the filter cache in the server's
     * memory [bytes].
     */
    uint64_t filter_cache_size;

    uint64_t translation_cache_brutto_size;
    uint64_t http_cache_brutto_size;
    uint64_t filter_cache_brutto_size;

    uint64_t nfs_cache_size, nfs_cache_brutto_size;

    /**
     * Total size of I/O buffers.
     */
    uint64_t io_buffers_size, io_buffers_brutto_size;

    /**
     * In- and outgoing HTTP traffic since
     * the server was started.
     */
    uint64_t http_traffic_received;
    uint64_t http_traffic_sent;
};

struct Header {
    uint16_t length;
    uint16_t command;
};

/**
 * This magic number precedes every UDP packet.
 */
static const uint32_t MAGIC = 0x63046101;

} // namespace BengControl
