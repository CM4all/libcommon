// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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
     * Deprecated.
     */
    NODE_STATUS = 4,

    /**
     * Deprecated.
     */
    DUMP_POOLS = 5,

    /**
     * Deprecated (in favor of the Prometheus exporter).
     */
    STATS = 6,

    /**
     * Set the logger verbosity.  The payload is one byte: 0 means
     * quiet, 1 is the default, and bigger values make the daemon more
     * verbose.
     */
    VERBOSE = 7,

    /**
     * Fade out all child processes (FastCGI, WAS, LHTTP, Delegate).
     * These will not be used for new
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
     * Deprecated (because userspace NFS support was removed).
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

    /**
     * Reload the state from class #StateDirectories and apply it to
     * the current process.
     */
    RELOAD_STATE = 19,

    /**
     * Disconnect all database connections matching the payload.  This
     * is usually received and handled by myproxy
     * (https://github.com/CM4all/myproxy) processes and the payload
     * is the account identifier.
     */
    DISCONNECT_DATABASE = 20,

    /**
     * Disable io_uring (temporarily).  Optional payload is a
     * big-endian 32 bit integer containing the number of seconds
     * after which it will be re-enabled automatically.  As this
     * overrides any previous #DISABLE_URING command, zero explicitly
     * re-enables io_uring now.
     */
    DISABLE_URING = 21,

    /**
     * Reset data structures bound to the specified account that keep
     * track of resource usage limits.  This shall be sent after
     * resource limits have been changed and applies only to data
     * structures that cannot automatically apply these because they
     * do not have enough context (e.g. token buckets).
     */
    RESET_LIMITER = 22,
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
