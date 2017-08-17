/*
 * Copyright 2007-2017 Content Management AG
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

#ifndef BENG_PROXY_TRANSLATION_PROTOCOL_HXX
#define BENG_PROXY_TRANSLATION_PROTOCOL_HXX

#include <stdint.h>

enum class TranslationCommand : uint16_t {
    /**
     * Beginning of a request/response.  The optional payload is a
     * uint8_t specifying the protocol version.
     */
    BEGIN = 1,

    END = 2,
    HOST = 3,
    URI = 4,
    STATUS = 5,
    PATH = 6,
    CONTENT_TYPE = 7,
    HTTP = 8,
    REDIRECT = 9,
    FILTER = 10,
    PROCESS = 11,
    SESSION = 12,
    PARAM = 13,
    USER = 14,
    LANGUAGE = 15,
    REMOTE_HOST = 16,
    PATH_INFO = 17,
    SITE = 18,
    CGI = 19,
    DOCUMENT_ROOT = 20,
    WIDGET_TYPE = 21,
    CONTAINER = 22,
    ADDRESS = 23,
    ADDRESS_STRING = 24,
    JAILCGI = 26,
    INTERPRETER = 27,
    ACTION = 28,
    SCRIPT_NAME = 29,
    AJP = 30,

    /** deprecated */
    DOMAIN_ = 31,

    STATEFUL = 32,
    FASTCGI = 33,
    VIEW = 34,
    USER_AGENT = 35,
    MAX_AGE = 36,
    VARY = 37,
    QUERY_STRING = 38,
    PIPE = 39,
    BASE = 40,
    DELEGATE = 41,
    INVALIDATE = 42,
    LOCAL_ADDRESS = 43,
    LOCAL_ADDRESS_STRING = 44,
    APPEND = 45,
    DISCARD_SESSION = 46,
    SCHEME = 47,
    REQUEST_HEADER_FORWARD = 48,
    RESPONSE_HEADER_FORWARD = 49,
    DEFLATED = 50,
    GZIPPED = 51,
    PAIR = 52,
    UNTRUSTED = 53,
    BOUNCE = 54,
    ARGS = 55,

    /**
     * The value of the "WWW-Authenticate" HTTP response header.
     */
    WWW_AUTHENTICATE = 56,

    /**
     * The value of the "Authentication-Info" HTTP response header.
     */
    AUTHENTICATION_INFO = 57,

    /**
     * The value of the "Authorization" HTTP request header.
     */
    AUTHORIZATION = 58,

    /**
     * A custom HTTP response header sent to the client.
     */
    HEADER = 59,

    UNTRUSTED_PREFIX = 60,

    /**
     * Set the "secure" flag on the session cookie.
     */
    SECURE_COOKIE = 61,

    /**
     * Enable filtering of client errors (status 4xx).  Without this
     * flag, only successful responses (2xx) are filtered.  Only
     * useful when at least one FILTER was specified.
     */
    FILTER_4XX = 62,

    /**
     * Support for custom error documents.  In the response, this is a
     * flag which enables custom error documents (i.e. if the HTTP
     * response is not successful, the translation server is asked to
     * provide a custom error document).  In a request, it queries the
     * location of the error document.
     */
    ERROR_DOCUMENT = 63,

    /**
     * Response: causes beng-proxy to submit the same translation
     * request again, with this packet appended.  The current response
     * is remembered, to be used when the second response contains the
     * PREVIOUS packet.
     *
     * Request: repeated request after CHECK was received.  The server
     * may respond with PREVIOUS.
     */
    CHECK = 64,

    /**
     * Tells beng-proxy to use the resource address of the previous
     * translation response.
     */
    PREVIOUS = 65,

    /**
     * Launch a WAS application to handle the request.
     */
    WAS = 66,

    /**
     * The absolute location of the home directory of the site owner
     * (hosting account).
     */
    HOME = 67,

    /**
     * Specifies the session realm.  An existing session matches only
     * if its realm matches the current request's realm.
     */
    REALM = 68,

    UNTRUSTED_SITE_SUFFIX = 69,

    /**
     * Transparent proxy: forward URI arguments to the request handler
     * instead of using them.
     */
    TRANSPARENT = 70,

    /**
     * Make the resource address "sticky", i.e. attempt to forward all
     * requests of a session to the same worker.
     */
    STICKY = 71,

    /**
     * Enable header dumps for the widget: on a HTTP request, the
     * request and response headers will be logged.  Only for
     * debugging purposes.
     */
    DUMP_HEADERS = 72,

    /**
     * Override the cookie host name.  This host name is used for
     * storing and looking up cookies in the jar.  It is especially
     * useful for protocols that don't have a host name, such as CGI.
     */
    COOKIE_HOST = 73,

    /**
     * Run the CSS processor.
     */
    PROCESS_CSS = 74,

    /**
     * Rewrite CSS class names with a leading underscore?
     */
    PREFIX_CSS_CLASS = 75,

    /**
     * Default URI rewrite mode is base=widget mode=focus.
     */
    FOCUS_WIDGET = 76,

    /**
     * Absolute URI paths are considered relative to the base URI of
     * the widget.
     */
    ANCHOR_ABSOLUTE = 77,

    /**
     * Rewrite XML ids with a leading underscore?
     */
    PREFIX_XML_ID = 78,

    /**
     * Reuse a cached response only if the request \verb|URI| matches
     * the specified regular expression (Perl compatible).
     */
    REGEX = 79,

    /**
     * Don't apply the cached response if the request URI matches the
     * specified regular expression (Perl compatible).
     */
    INVERSE_REGEX = 80,

    /**
     * Run the text processor to expand entity references.
     */
    PROCESS_TEXT = 81,

    /**
     * Send widget metadata (id, prefix, type) to the widget server.
     */
    WIDGET_INFO = 82,

    /**
     * Expand #REGEX match strings in this PATH_INFO value.
     * Sub-strings in the form "\1" will be replaced.  It can be used
     * to copy URI parts to a filter.
     */
    EXPAND_PATH_INFO = 83,

    /**
     * Expand #REGEX match strings in this PATH value (only
     * CGI, FastCGI, WAS).  Sub-strings in the form "\1" will be
     * replaced.
     */
    EXPAND_PATH = 84,

    /**
     * Set the session cookie's "Domain" attribute.
     */
    COOKIE_DOMAIN = 85,

    /**
     * The URI of the "local" location of a widget class.  This may
     * refer to a location that serves static resources.  It is used
     * by the processor for rewriting URIs.
     */
    LOCAL_URI = 86,

    /**
     * Enable CGI auto-base.
     */
    AUTO_BASE = 87,

    /**
     * A classification for the User-Agent request header.
     */
    UA_CLASS = 88,

    /**
     * Shall the XML/HTML processor invoke the CSS processor for
     * "style" element contents?
     */
    PROCESS_STYLE = 89,

    /**
     * Does this widget support new-style direct URI addressing?
     *
     * Example: http://localhost/template.html;frame=foo/bar - this
     * requests the widget "foo" and with path-info "/bar".
     */
    DIRECT_ADDRESSING = 90,

    /**
     * Allow this widget to embed more instances of its own class.
     */
    SELF_CONTAINER = 91,

    /**
     * Allow this widget to embed instances of this group.  This can
     * be specified multiple times to allow more than one group.  It
     * can be combined with #SELF_CONTAINER.
     */
    GROUP_CONTAINER = 92,

    /**
     * Assign a group name to the widget type.  This is used by
     * #GROUP_CONTAINER.
     */
    WIDGET_GROUP = 93,

    /**
     * A cached response is valid only if the file specified in this
     * packet is not modified.
     *
     * The first 8 bytes is the mtime (seconds since UNIX epoch), the
     * rest is the absolute path to a regular file (symlinks not
     * supported).  The translation fails when the file does not exist
     * or is inaccessible.
     */
    VALIDATE_MTIME = 94,

    /**
     * Mount a NFS share.  This packet specifies the server (IP
     * address).
     */
    NFS_SERVER = 95,

    /**
     * Mount a NFS share.  This packet specifies the export path to be
     * mounted from the server.
     */
    NFS_EXPORT = 96,

    /**
     * The path of a HTTP server program that will be launched.
     */
    LHTTP_PATH = 97,

    /**
     * The URI that will be requested on the given HTTP server
     * program.
     */
    LHTTP_URI = 98,

    /**
     * Expand #REGEX match strings in this
     * #LHTTP_URI value.  Sub-strings in the form "\1" will
     * be replaced.
     */
    EXPAND_LHTTP_URI = 99,

    /**
     * The "Host" request header for the #LHTTP_PATH.
     */
    LHTTP_HOST = 100,

    /**
     * How many concurrent requests will be handled by the
     * aforementioned process?
     */
    CONCURRENCY = 101,

    /**
     * The translation server sends this packet when it wants to have
     * the full request URI.  beng-proxy then sends another
     * translation request, echoing this packet (including its
     * payload), and #URI containing the full request URI
     * (not including the query string)
     */
    WANT_FULL_URI = 102,

    /**
     * Start the child process in a new user namespace?
     */
    USER_NAMESPACE = 103,

    /**
     * Start the child process in a new network namespace?
     */
    NETWORK_NAMESPACE = 104,

    /**
     * Add expansion for the preceding #APPEND.
     */
    EXPAND_APPEND = 105,

    /**
     * Add expansion for the preceding #PAIR.
     */
    EXPAND_PAIR = 106,

    /**
     * Start the child process in a new PID namespace?
     */
    PID_NAMESPACE = 107,

    /**
     * Starts the child process in a new mount namespace and invokes
     * pivot_root().  Payload is the new root directory, which must
     * contain a directory called "mnt".
     */
    PIVOT_ROOT = 108,

    /**
     * Mount the proc filesystem on /proc?
     */
    MOUNT_PROC = 109,

    /**
     * Mount the specified home directory?  Payload is the mount
     * point.
     */
    MOUNT_HOME = 110,

    /**
     * Mount a new tmpfs on /tmp?
     */
    MOUNT_TMP_TMPFS = 111,

    /**
     * Create a new UTS namespace?  Payload is the host name inside
     * the namespace.
     */
    UTS_NAMESPACE = 112,

    /**
     * Bind-mount a directory.  Payload is source and target separated
     * by a null byte.
     */
    BIND_MOUNT = 113,

    /**
     * Set resource limits via setrlimit().
     */
    RLIMITS = 114,

    /**
     * The translation server wishes to have the specified data:
     * payload is an array of uint16_t containing translation
     * commands.
     */
    WANT = 115,

    /**
     * Modifier for #BASE: do not perform any safety checks
     * on the tail string.
     */
    UNSAFE_BASE = 116,

    /**
     * Enables "easy" mode for #BASE or
     * #UNSAFE_BASE: the returned resource address refers to
     * the base, not to the actual request URI.
     */
    EASY_BASE = 117,

    /**
     * Apply #REGEX and #INVERSE_REGEX to the
     * remaining URI following #BASE instead of the whole
     * request URI?
     */
    REGEX_TAIL = 118,

    /**
     * Unescape the URI for #REGEX and
     * #INVERSE_REGEX?
     */
    REGEX_UNESCAPE = 119,

    /**
     * Retranslate if the file does not exist.
     */
    FILE_NOT_FOUND = 120,

    /**
     * Translation server indicates that Content-Type lookup should be
     * performed for static files.  Upon request, this packet is
     * echoed to the translation server, accompanied by a
     * #SUFFIX packet.
     */
    CONTENT_TYPE_LOOKUP = 121,

    /**
     * Payload is the file name suffix without the dot.  Part of a
     * #CONTENT_TYPE_LOOKUP translation request.
     */
    SUFFIX = 122,

    /**
     * Retranslate if the file is a directory.
     */
    DIRECTORY_INDEX = 123,

    /**
     * Generate an "Expires" header for static files.  Payload is a 32
     * bit integer specifying the number of seconds from now on.
     */
    EXPIRES_RELATIVE = 124,

    EXPAND_REDIRECT = 125,

    EXPAND_SCRIPT_NAME = 126,

    /**
     * Override the path to be tested by #FILE_NOT_FOUND.
     */
    TEST_PATH = 127,

    /**
     * Expansion for #TEST_PATH.
     */
    EXPAND_TEST_PATH = 128,

    /**
     * Copy the query string to the redirect URI?
     */
    REDIRECT_QUERY_STRING = 129,

    /**
     * Negotiate how to handle requests to regular file with
     * path info.
     */
    ENOTDIR_ = 130,

    /**
     * An absolute path where STDERR output will be appended.
     */
    STDERR_PATH = 131,

    /**
     * Set the session cookie's "Path" attribute.
     */
    COOKIE_PATH = 132,

    /**
     * Advanced authentication protocol through the translation
     * server.
     */
    AUTH = 133,

    /**
     * Set an evironment variable.  Unlike #PAIR, this works
     * even for FastCGI and WAS.
     */
    SETENV = 134,

    /**
     * Expansion for #SETENV.
     */
    EXPAND_SETENV = 135,

    /**
     * Expansion for #URI.
     */
    EXPAND_URI = 136,

    /**
     * Expansion for #SITE.
     */
    EXPAND_SITE = 137,

    /**
     * Send an addtional request header to the backend server.
     */
    REQUEST_HEADER = 138,

    /**
     * Expansion for #REQUEST_HEADER.
     */
    EXPAND_REQUEST_HEADER = 139,

    /**
     * Build the "gzipped" path automatically by appending ".gz" to
     * the "regular" path.
     */
    AUTO_GZIPPED = 140,

    /**
     * Expansion for #DOCUMENT_ROOT.
     */
    EXPAND_DOCUMENT_ROOT = 141,

    /**
     * Check if the #TEST_PATH (or
     * #EXPAND_TEST_PATH) plus one of the suffixes from
     * #PROBE_SUFFIX exists (regular files only).
     * beng-proxy will send another translation request, echoing this
     * packet and echoing the #PROBE_SUFFIX that was found.
     *
     * This packet must be followed by at least two
     * #PROBE_SUFFIX packets.
     */
    PROBE_PATH_SUFFIXES = 142,

    /**
     * @see #PROBE_PATH_SUFFIXES
     */
    PROBE_SUFFIX = 143,

    /**
     * Load #AUTH from a file.
     */
    AUTH_FILE = 144,

    /**
     * Expansion for #AUTH_FILE.
     */
    EXPAND_AUTH_FILE = 145,

    /**
     * Append the payload to #AUTH_FILE data.
     */
    APPEND_AUTH = 146,

    /**
     * Expansion for #APPEND_AUTH.
     */
    EXPAND_APPEND_AUTH = 147,

    /**
     * Indicates which listener accepted the connection.
     */
    LISTENER_TAG = 148,

    /**
     * Expansion for #COOKIE_HOST.
     */
    EXPAND_COOKIE_HOST = 149,

    /**
     * Expansion for #BIND_MOUNT.
     */
    EXPAND_BIND_MOUNT = 150,

    /**
     * Pass non-blocking socket to child process?
     */
    NON_BLOCKING = 151,

    /**
     * Read a file and return its contents to the translation server.
     */
    READ_FILE = 152,

    /**
     * Expansion for #READ_FILE.
     */
    EXPAND_READ_FILE = 153,

    /**
     * Expansion for #HEADER.
     */
    EXPAND_HEADER = 154,

    /**
     * If present, the use HOST+URI as input for #REGEX and
     * not just the URI.
     */
    REGEX_ON_HOST_URI = 155,

    /**
     * Set a session-wide site name.
     */
    SESSION_SITE = 156,

    /**
     * Start the child process in a new IPC namespace?
     */
    IPC_NAMESPACE = 157,

    /**
     * Deflate the response on-the-fly if the client accepts it.
     */
    AUTO_DEFLATE = 158,

    /**
     * Expansion for #HOME.
     */
    EXPAND_HOME = 159,

    /**
     * Expansion for #STDERR_PATH.
     */
    EXPAND_STDERR_PATH = 160,

    /**
     * If present, the use USER+'@'+URI as input for #REGEX
     * and not just the URI.
     */
    REGEX_ON_USER_URI = 161,

    /**
     * Gzip-compress the response on-the-fly if the client accepts it.
     */
    AUTO_GZIP = 162,

    /**
     * Re-translate with the URI specified by #URI or
     * #EXPAND_URI.
     */
    INTERNAL_REDIRECT = 163,

    /**
     * Obtain information for interactive login.  Must be followed by
     * #USER.
     */
    LOGIN = 164,

    /**
     * Specify uid and gid (and supplementary groups) for the child
     * process.  Payload is an array of 32 bit integers.
     */
    UID_GID = 165,

    /**
     * A password for #LOGIN / #USER that shall be
     * verified by the translation server.
     */
    PASSWORD = 166,

    /**
     * Configure a refence limit for the child process.
     */
    REFENCE = 167,

    /**
     * Payload specifies the service that wants to log in (see
     * #LOGIN), e.g. "ssh" or "ftp".
     */
    SERVICE = 168,

    /**
     * Unescape the URI for #INVERSE_REGEX?
     */
    INVERSE_REGEX_UNESCAPE = 169,

    /**
     * Same as #BIND_MOUNT, but don't set the "read-only" flag.
     */
    BIND_MOUNT_RW = 170,

    /**
     * Same as #EXPAND_BIND_MOUNT, but don't set the
     * "read-only" flag.
     */
    EXPAND_BIND_MOUNT_RW = 171,

    UNTRUSTED_RAW_SITE_SUFFIX = 172,

    /**
     * Mount a new tmpfs on the given path.
     */
    MOUNT_TMPFS = 173,

    /**
     * Send the X-CM4all-BENG-User header to the filter?
     */
    REVEAL_USER = 174,

    /**
     * Copy #AUTH or #AUTH_FILE (without
     * #APPEND_AUTH) to #REALM
     */
    REALM_FROM_AUTH_BASE = 175,

    /**
     * Permanently disable new privileges for the child process.
     */
    NO_NEW_PRIVS = 176,

    /**
     * Move the child process into a cgroup (payload is the cgroup's
     * base name).
     */
    CGROUP = 177,

    /**
     * Set a cgroup attribute.  Payload is in the form
     * "controller.name=value", e.g. "cpu.shares=42".
     */
    CGROUP_SET = 178,

    /**
     * A http:// URL for this session in an external session manager.
     * GET refreshes the session
     * (#EXTERNAL_SESSION_KEEPALIVE), DELETE discards it
     * (#DISCARD_SESSION).
     */
    EXTERNAL_SESSION_MANAGER = 179,

    /**
     * 16 bit integer specifying the number of seconds between to
     * refresh (GET) calls on #EXTERNAL_SESSION_MANAGER.
     */
    EXTERNAL_SESSION_KEEPALIVE = 180,

    /**
     * Mark this request as a "cron job" request.  No payload.
     */
    CRON = 181,

    /**
     * Same as #BIND_MOUNT, but don't set the "noexec" flag.
     */
    BIND_MOUNT_EXEC = 182,

    /**
     * Same as #EXPAND_BIND_MOUNT, but don't set the
     * "noexec" flag.
     */
    EXPAND_BIND_MOUNT_EXEC = 183,

    /**
     * Redirect STDERR to /dev/null?
     */
    STDERR_NULL = 184,

    /**
     * Execute the specified program.  May be followed by
     * #APPEND packets.  This is used by Workshop/Cron.
     */
    EXECUTE = 185,

    /**
     * Forbid the child process to create new user namespaces.
     */
    FORBID_USER_NS = 186,

    /**
     * Request: ask the translation server which configured pool to
     * send this HTTP request to.  Payload is the translation_handler
     * name (may be empty, though).
     *
     * Response: payload specifies the pool name.
     */
    POOL = 187,

    /**
     * Payload is a "text/plain" response body.  It should be short
     * and US-ASCII.
     */
    MESSAGE = 188,

    /**
     * Payload is the canonical name for this host, to be used instead
     * of the "Host" request header.  Its designed use is
     * StickyMode::HOST.
     */
    CANONICAL_HOST = 189,

    /**
     * An absolute path specifying the user's shell (for
     * #LOGIN).
     */
    SHELL = 190,

    /**
     * An opaque token passed from the translation server to the
     * software (e.g. to be evaluated by a frontend script or to be
     * matched by a configuration file).
     */
    TOKEN = 191,

    /**
     * Like #STDERR_PATH, but open the file after entering the jail.
     */
    STDERR_PATH_JAILED = 192,

    /**
     * The umask for the new child process.  Payload is a 16 bit
     * integer.
     */
    UMASK = 193,

    /**
     * Start the child process in a new Cgroup namespace?
     */
    CGROUP_NAMESPACE = 194,

    /**
     * Use the full request URI for #REDIRECT?  This should be used
     * with #REDIRECT, #BASE, #EASY_BASE and #REDIRECT_QUERY_STRING.
     */
    REDIRECT_FULL_URI = 195,

    /**
     * Forbid the child process to add multicast group memberships.
     */
    FORBID_MULTICAST = 196,

    /**
     * Allow only HTTPS, and generate a redirect to a https:// if this
     * is plain http://.  Optional payload is a HTTPS port number (16
     * bit).
     */
    HTTPS_ONLY = 197,

    /**
     * Forbid the child process to invoke the bind() and listen()
     * system calls.
     */
    FORBID_BIND = 198,

    /**
     * Reassociate with the given named network namespace.
     */
    NETWORK_NAMESPACE_NAME = 199,

    /**
     * Mount a tmpfs to "/"?
     */
    MOUNT_ROOT_TMPFS = 200,
};

struct TranslationHeader {
    uint16_t length;
    TranslationCommand command;
};

static_assert(sizeof(TranslationHeader) == 4, "Wrong size");

#endif
