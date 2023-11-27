// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>

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

	/** deprecated */
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
	 * Obsolete.
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

	/** deprecated */
	NFS_SERVER = 95,

	/** deprecated */
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
	 * Obsolete.
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
	 * Mount a new (writable) tmpfs on the given path.
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
	 * Request: the client wants to know how to execute the
	 * specified program; payload is a token describing the
	 * program.
	 *
	 * Response: execute the specified program.  May be followed by
	 * #APPEND packets.
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

	/**
	 * A "tag" string for the child process.  This can be used to
	 * address groups of child processes.
	 */
	CHILD_TAG = 201,

	/**
	 * The name of the SSL/TLS client certificate to be used.
	 */
	CERTIFICATE = 202,

	/**
	 * Disable the HTTP cache for the given address.
	 */
	UNCACHED = 203,

	/**
	 * Reassociate with the given named PID namespace (queried from
	 * the Spawn daemon).
	 */
	PID_NAMESPACE_NAME = 204,

	/**
	 * Substitute variables in the form "{%NAME%}" with values from
	 * the given YAML file.
	 */
	SUBST_YAML_FILE = 205,

	/**
	 * The value of the "X-CM4all-AltHost" request header (if enabled
	 * on the listener).  Only used for #AUTH requests.
	 */
	ALT_HOST = 206,

	/**
	 * Use an alternative syntax for substitutions
	 * (e.g. #SUBST_YAML_FILE).
	 */
	SUBST_ALT_SYNTAX = 207,

	/**
	 * An opaque tag string to be assigned to the cache item (if the
	 * response is going to be cached).
	 */
	CACHE_TAG = 208,

	/**
	 * Require a valid "X-CM4all-CSRF-Token" header for modifying
	 * requests (POST etc.).
	 */
	REQUIRE_CSRF_TOKEN = 209,

	/**
	 * A valid "X-CM4all-CSRF-Token" header will be added to the
	 * response.
	 */
	SEND_CSRF_TOKEN = 210,

	/**
	 * Force the #HTTP address to be HTTP/2.
	 */
	HTTP2 = 211,

	/**
	 * Pass the CGI parameter "REQUEST_URI" verbatim instead of
	 * building it from SCRIPT_NAME, PATH_INFO and QUERY_STRING.
	 */
	REQUEST_URI_VERBATIM = 212,

	/**
	 * Defer the request to another translation server.
	 */
	DEFER = 213,

	/**
	 * Send the child's STDERR output to the configured Pond
	 * server instead of to systemd-journald.
	 */
	STDERR_POND = 214,

	/**
	 * Enable request chaining: after the HTTP response is
	 * received, another translation is requested echoing this
	 * #CHAIN packet.  The translation server provides another
	 * HTTP request handler to which the previous response will be
	 * sent as a POST request.
	 */
	CHAIN = 215,

	/**
	 * Stop the current chain and deliver the pending response to
	 * the client of the initial HTTP request.
	 */
	BREAK_CHAIN = 216,

	/**
	 * The value of the "X-CM4all-Chain" response header in #CHAIN
	 * requests.
	 */
	CHAIN_HEADER = 217,

	/**
	 * Option for #FILTER: don't send a request body to the
	 * filter, and discard successful responses from the filter.
	 */
	FILTER_NO_BODY = 218,

	/**
	 * Require HTTP-based authentication.
	 */
	HTTP_AUTH = 219,

	/**
	 * Enable token-based authentication (with the query string
	 * parameter "auth_token").
	 */
	TOKEN_AUTH = 220,

	/**
	 * The (unescaped) value of the "auth_token" query string
	 * parameter (with #TOKEN_AUTH).
	 */
	AUTH_TOKEN = 221,

	/**
	 * Mount a new (read-only) tmpfs on the given path.
	 */
	MOUNT_EMPTY = 222,

	/**
	 * Generate a response with a tiny (one-pixel GIF) image.
	 */
	TINY_IMAGE = 223,

	/**
	 * All sessions with the given identifier are merged.
	 */
	ATTACH_SESSION = 224,

	/**
	 * Like #DISCARD_SESSION, but discard only the part of the
	 * session specific to this realm.
	 */
	DISCARD_REALM_SESSION = 225,

	/**
	 * Repeat the translation, but with the specified HOST value.
	 */
	LIKE_HOST = 226,

	/**
	 * The translation server gives an overview of the URI layout,
	 * responding with a list of #BASE packets.  The payload is
	 * opaque and will be mirrored in the following request.
	 */
	LAYOUT = 227,

	/**
	 * A cookie value which allows the translation to recover
	 * (part of) the session if beng-proxy has lost the session
	 * (or the request arrives at a different worker).
	 */
	RECOVER_SESSION = 228,

	/**
	 * Specifies that the previous command is optional.  Errors
	 * because an object was not found are not fatal.
	 */
	OPTIONAL = 229,

	/**
	 * Look for a brotli-compressed file by appending ".br" to the
	 * path.
	 */
	AUTO_BROTLI_PATH = 230,

	/**
	 * Enable "transparent" mode for #CHAIN.
	 */
	TRANSPARENT_CHAIN = 231,

	/**
	 * Collect statistics for this request under the given tag.
	 */
	STATS_TAG = 232,

	/**
	 * Mount a minimalistic /dev (but can be implemented by
	 * bind-mounting the host /dev into the new container).  This
	 * is useful for #MOUNT_EMPTY.
	 */
	MOUNT_DEV = 233,

	/**
	 * Bind-mount a (read-only) file.  Payload is source and
	 * target separated by a null byte.
	 */
	BIND_MOUNT_FILE = 234,

	/**
	 * The HTTP response should be cached even if it does not have
	 * headers declaring its cacheability.
	 */
	EAGER_CACHE = 235,

	/**
	 * All (successful) modifying requests (POST, PUT ...) flush
	 * the HTTP cache of the specified #CACHE_TAG.
	 */
	AUTO_FLUSH_CACHE = 236,

	/**
	 * Lauch how many child processes of this kind?  This is
	 * similar to #CONCURRENCY, but at the process level, not at
	 * the connection level.
	 */
	PARALLELISM = 237,

	/**
	 * Like #EXPIRES_RELATIVE, but this value is only used if
	 * there is a non-empty query string.
	 */
	EXPIRES_RELATIVE_WITH_QUERY = 238,

	/**
	 * Set a cgroup extended attribute.  Payload is in the form
	 * "user.name=value", e.g. "user.account_id=42".
	 */
	CGROUP_XATTR = 239,

	/**
	 * A #CHECK request shall include the value of the specified
	 * request header.
	 */
	CHECK_HEADER = 240,

	/**
	 * The name of the Workshop
	 * (https://github.com/CM4all/workshop/) plan which triggered
	 * this request.
	 */
	PLAN = 241,

	/**
	 * Change the current directory.
	 */
	CHDIR = 242,

	/**
	 * Set the "SameSite" attribute on the session cookie.
	 */
	SESSION_COOKIE_SAME_SITE = 243,

	/**
	 * The #LOGIN request can be approved without a password.  An
	 * optional payload may describe a service-specific
	 * limitation, e.g. "sftp" to limit LOGIN/SERVICE=ssh to
	 * SERVICE=sftp.
	 */
	NO_PASSWORD = 244,

	/**
	 * Like #SESSION, but realm-local.  Unlike #SESSION, it is
	 * only sent under certain conditions (e.g. in #TOKEN_AUTH
	 * requests), because the realm is only known after the
	 * regular translation response has been applied already.
	 */
	REALM_SESSION = 245,

	/**
	 * Write a file in a mount namespace.  Payload is the absolute
	 * path and the file contents separated by a null byte.
	 */
	WRITE_FILE = 246,

	/**
	 * Resubmit the translation with this packet and a #STATUS
	 * packet describing whether the path (from #PATH) exists.
	 */
	PATH_EXISTS = 247,

	/**
	 * The contents of an OpenSSH authorized_keys file.
	 */
	AUTHORIZED_KEYS = 248,

	/**
	 * Compress the response the-fly with Brotli if the client
	 * accepts it.
	 */
	AUTO_BROTLI = 249,

	/**
	 * Mark the child process as "disposable", which may give it a
	 * very short idle timeout (or none at all).  To be used for
	 * processes that will likely only be used once.
	 */
	DISPOSABLE = 250,

	/**
	 * Discard the query string from the request URI.  This can be
	 * combined with #EAGER_CACHE to prevent cache-busting with
	 * random query strings.
	 */
	DISCARD_QUERY_STRING = 251,

	/**
	 * Mount a shared (writable) tmpfs on the given path.  Payload
	 * is the name of the tmpfs and the target path separated by a
	 * null byte.
	 */
	MOUNT_NAMED_TMPFS = 252,

	/**
	 * Limit file access to files beneath this directory.
	 */
	BENEATH = 253,

	/**
	 * Like #UID_GID, but these are the numbers visible inside the
	 * user namespace.
	 *
	 * Currently, only the uid is implemented, therefore the
	 * payload must be a 32-bit integer.
	 */
	MAPPED_UID_GID = 254,

	/**
	 * If present, then ~/.ssh/authorized_keys is not used.
	 */
	NO_HOME_AUTHORIZED_KEYS = 255,
};

struct TranslationHeader {
	uint16_t length;
	TranslationCommand command;
};

static_assert(sizeof(TranslationHeader) == 4, "Wrong size");
