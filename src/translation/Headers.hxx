// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Definitions for the beng-proxy translation protocol.
 */

#pragma once

#include <cstdint>

namespace BengProxy {

/**
 * How is a specific set of headers forwarded?
 */
enum class HeaderForwardMode : uint8_t {
    /**
     * Do not forward at all.
     */
    NO,

    /**
     * Forward it as-is.
     */
    YES,

    /**
     * Forward it, but mangle it.  Example: cookie headers are handled
     * by beng-proxy.
     */
    MANGLE,

    /**
     * A special "mixed" mode where both beng-proxy and the backend
     * server handle certain headers.
     */
    BOTH,
};

/**
 * Selectors for a group of headers.
 */
enum class HeaderGroup {
    /**
     * Special value for "override all settings" (except for
     * #SECURE and #LINK).
     */
    ALL = -1,

    /**
     * Reveal the identity of the real communication partner?  This
     * affects "Via", "X-Forwarded-For".
     */
    IDENTITY,

    /**
     * Forward headers showing the capabilities of the real
     * communication partner?  Affects "Server", "User-Agent",
     * "Accept-*" and others.
     *
     * Note that the "Server" response header is always sent, even
     * when this attribute is set to #HEADER_FORWARD_NO.
     */
    CAPABILITIES,

    /**
     * Forward cookie headers?
     */
    COOKIE,

    /**
     * Forwarding mode for "other" headers: headers not explicitly
     * handled here.  This does not include hop-by-hop headers.
     */
    OTHER,

    /**
     * Forward information about the original request/response that
     * would usually not be visible.  If set to
     * #HEADER_FORWARD_MANGLE, then "Host" is translated to
     * "X-Forwarded-Host".
     */
    FORWARD,

    /**
     * Forward CORS headers.
     *
     * @see http://www.w3.org/TR/cors/#syntax
     */
    CORS,

    /**
     * Forward "secure" headers such as "x-cm4all-beng-user".
     */
    SECURE,

    /**
     * Forward headers that affect the transformation, such as
     * "x-cm4all-view".
     */
    TRANSFORMATION,

    /**
     * Forward headers that contain links, such as "location".
     */
    LINK,

    /**
     * Information about the SSL connection,
     * i.e. X-CM4all-BENG-Peer-Subject and
     * X-CM4all-BENG-Peer-Issuer-Subject.
     */
    SSL,

    /**
     * Forward authentication headers such as "WWW-Authenticate" and
     * "Authorization".
     */
    AUTH,

    /**
     * Internal definition for estimating the size of an array.
     */
    MAX,
};

struct HeaderForwardPacket {
    /**
     * See #beng_header_group
     */
    int16_t group;

    /**
     * See #beng_header_forward_mode
     */
    uint8_t mode;

    /**
     * Unused padding byte.  Set 0.
     */
    uint8_t reserved;
};

static_assert(sizeof(HeaderForwardPacket) == 4, "Wrong struct size");

} // namespace BengProxy
