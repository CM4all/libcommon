// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "String.hxx"
#include "Protocol.hxx"

#include <algorithm> // for std::find()
#include <array>
#include <string_view>
#include <utility>

using std::string_view_literals::operator""sv;

// Index-based array where the index is the enum value and the value is the string name
// Zero-initialized entries represent unknown/unsupported commands
constexpr auto translation_command_names = std::array{
	""sv, // 0 - unused
	"BEGIN"sv, // 1
	"END"sv, // 2
	"HOST"sv, // 3
	"URI"sv, // 4
	"STATUS"sv, // 5
	"PATH"sv, // 6
	"CONTENT_TYPE"sv, // 7
	"HTTP"sv, // 8
	"REDIRECT"sv, // 9
	"FILTER"sv, // 10
	"PROCESS"sv, // 11
	"SESSION"sv, // 12
	"PARAM"sv, // 13
	"USER"sv, // 14
	"LANGUAGE"sv, // 15
	"REMOTE_HOST"sv, // 16
	"PATH_INFO"sv, // 17
	"SITE"sv, // 18
	"CGI"sv, // 19
	"DOCUMENT_ROOT"sv, // 20
	"WIDGET_TYPE"sv, // 21
	"CONTAINER"sv, // 22
	"ADDRESS"sv, // 23
	"ADDRESS_STRING"sv, // 24
	""sv, // 25 - unused
	"JAILCGI"sv, // 26
	"INTERPRETER"sv, // 27
	"ACTION"sv, // 28
	"SCRIPT_NAME"sv, // 29
	"AJP"sv, // 30
	""sv, // 31 - DOMAIN_ (deprecated)
	"STATEFUL"sv, // 32
	"FASTCGI"sv, // 33
	"VIEW"sv, // 34
	"USER_AGENT"sv, // 35
	"MAX_AGE"sv, // 36
	"VARY"sv, // 37
	"QUERY_STRING"sv, // 38
	"PIPE"sv, // 39
	"BASE"sv, // 40
	""sv, // 41 - DELEGATE (deprecated)
	"INVALIDATE"sv, // 42
	"LOCAL_ADDRESS"sv, // 43
	"LOCAL_ADDRESS_STRING"sv, // 44
	"APPEND"sv, // 45
	"DISCARD_SESSION"sv, // 46
	"SCHEME"sv, // 47
	"REQUEST_HEADER_FORWARD"sv, // 48
	"RESPONSE_HEADER_FORWARD"sv, // 49
	""sv, // 50 - DEFLATED (deprecated)
	"GZIPPED"sv, // 51
	"PAIR"sv, // 52
	"UNTRUSTED"sv, // 53
	"BOUNCE"sv, // 54
	"ARGS"sv, // 55
	"WWW_AUTHENTICATE"sv, // 56
	"AUTHENTICATION_INFO"sv, // 57
	"AUTHORIZATION"sv, // 58
	"HEADER"sv, // 59
	"UNTRUSTED_PREFIX"sv, // 60
	"SECURE_COOKIE"sv, // 61
	"FILTER_4XX"sv, // 62
	"ERROR_DOCUMENT"sv, // 63
	"CHECK"sv, // 64
	"PREVIOUS"sv, // 65
	"WAS"sv, // 66
	"HOME"sv, // 67
	"REALM"sv, // 68
	"UNTRUSTED_SITE_SUFFIX"sv, // 69
	"TRANSPARENT"sv, // 70
	"STICKY"sv, // 71
	"DUMP_HEADERS"sv, // 72
	"COOKIE_HOST"sv, // 73
	"PROCESS_CSS"sv, // 74
	"PREFIX_CSS_CLASS"sv, // 75
	"FOCUS_WIDGET"sv, // 76
	"ANCHOR_ABSOLUTE"sv, // 77
	"PREFIX_XML_ID"sv, // 78
	"REGEX"sv, // 79
	"INVERSE_REGEX"sv, // 80
	"PROCESS_TEXT"sv, // 81
	"WIDGET_INFO"sv, // 82
	"EXPAND_PATH_INFO"sv, // 83
	"EXPAND_PATH"sv, // 84
	"COOKIE_DOMAIN"sv, // 85
	"LOCAL_URI"sv, // 86
	"AUTO_BASE"sv, // 87
	"UA_CLASS"sv, // 88
	"PROCESS_STYLE"sv, // 89
	"DIRECT_ADDRESSING"sv, // 90
	"SELF_CONTAINER"sv, // 91
	"GROUP_CONTAINER"sv, // 92
	"WIDGET_GROUP"sv, // 93
	"VALIDATE_MTIME"sv, // 94
	"NFS_SERVER"sv, // 95
	"NFS_EXPORT"sv, // 96
	"LHTTP_PATH"sv, // 97
	"LHTTP_URI"sv, // 98
	"EXPAND_LHTTP_URI"sv, // 99
	"LHTTP_HOST"sv, // 100
	"CONCURRENCY"sv, // 101
	"WANT_FULL_URI"sv, // 102
	"USER_NAMESPACE"sv, // 103
	"NETWORK_NAMESPACE"sv, // 104
	"EXPAND_APPEND"sv, // 105
	"EXPAND_PAIR"sv, // 106
	"PID_NAMESPACE"sv, // 107
	"PIVOT_ROOT"sv, // 108
	"MOUNT_PROC"sv, // 109
	"MOUNT_HOME"sv, // 110
	"MOUNT_TMP_TMPFS"sv, // 111
	"UTS_NAMESPACE"sv, // 112
	"BIND_MOUNT"sv, // 113
	"RLIMITS"sv, // 114
	"WANT"sv, // 115
	"UNSAFE_BASE"sv, // 116
	"EASY_BASE"sv, // 117
	"REGEX_TAIL"sv, // 118
	"REGEX_UNESCAPE"sv, // 119
	"FILE_NOT_FOUND"sv, // 120
	"CONTENT_TYPE_LOOKUP"sv, // 121
	"SUFFIX"sv, // 122
	"DIRECTORY_INDEX"sv, // 123
	"EXPIRES_RELATIVE"sv, // 124
	"EXPAND_REDIRECT"sv, // 125
	"EXPAND_SCRIPT_NAME"sv, // 126
	"TEST_PATH"sv, // 127
	"EXPAND_TEST_PATH"sv, // 128
	"REDIRECT_QUERY_STRING"sv, // 129
	""sv, // 130 - ENOTDIR_ (deprecated)
	"STDERR_PATH"sv, // 131
	"COOKIE_PATH"sv, // 132
	"AUTH"sv, // 133
	"SETENV"sv, // 134
	"EXPAND_SETENV"sv, // 135
	"EXPAND_URI"sv, // 136
	"EXPAND_SITE"sv, // 137
	"REQUEST_HEADER"sv, // 138
	"EXPAND_REQUEST_HEADER"sv, // 139
	"AUTO_GZIPPED"sv, // 140
	"EXPAND_DOCUMENT_ROOT"sv, // 141
	"PROBE_PATH_SUFFIXES"sv, // 142
	"PROBE_SUFFIX"sv, // 143
	"AUTH_FILE"sv, // 144
	"EXPAND_AUTH_FILE"sv, // 145
	"APPEND_AUTH"sv, // 146
	"EXPAND_APPEND_AUTH"sv, // 147
	"LISTENER_TAG"sv, // 148
	"EXPAND_COOKIE_HOST"sv, // 149
	"EXPAND_BIND_MOUNT"sv, // 150
	"NON_BLOCKING"sv, // 151
	"READ_FILE"sv, // 152
	"EXPAND_READ_FILE"sv, // 153
	"EXPAND_HEADER"sv, // 154
	"REGEX_ON_HOST_URI"sv, // 155
	"SESSION_SITE"sv, // 156
	"IPC_NAMESPACE"sv, // 157
	"AUTO_DEFLATE"sv, // 158
	"EXPAND_HOME"sv, // 159
	"EXPAND_STDERR_PATH"sv, // 160
	"REGEX_ON_USER_URI"sv, // 161
	"AUTO_GZIP"sv, // 162
	"INTERNAL_REDIRECT"sv, // 163
	"LOGIN"sv, // 164
	"UID_GID"sv, // 165
	"PASSWORD"sv, // 166
	"REFENCE"sv, // 167
	"SERVICE"sv, // 168
	"INVERSE_REGEX_UNESCAPE"sv, // 169
	"BIND_MOUNT_RW"sv, // 170
	"EXPAND_BIND_MOUNT_RW"sv, // 171
	"UNTRUSTED_RAW_SITE_SUFFIX"sv, // 172
	"MOUNT_TMPFS"sv, // 173
	"REVEAL_USER"sv, // 174
	"REALM_FROM_AUTH_BASE"sv, // 175
	"NO_NEW_PRIVS"sv, // 176
	"CGROUP"sv, // 177
	"CGROUP_SET"sv, // 178
	"EXTERNAL_SESSION_MANAGER"sv, // 179
	"EXTERNAL_SESSION_KEEPALIVE"sv, // 180
	"CRON"sv, // 181
	"BIND_MOUNT_EXEC"sv, // 182
	"EXPAND_BIND_MOUNT_EXEC"sv, // 183
	"STDERR_NULL"sv, // 184
	"EXECUTE"sv, // 185
	"FORBID_USER_NS"sv, // 186
	"POOL"sv, // 187
	"MESSAGE"sv, // 188
	"CANONICAL_HOST"sv, // 189
	"SHELL"sv, // 190
	"TOKEN"sv, // 191
	"STDERR_PATH_JAILED"sv, // 192
	"UMASK"sv, // 193
	"CGROUP_NAMESPACE"sv, // 194
	"REDIRECT_FULL_URI"sv, // 195
	"FORBID_MULTICAST"sv, // 196
	"HTTPS_ONLY"sv, // 197
	"FORBID_BIND"sv, // 198
	"NETWORK_NAMESPACE_NAME"sv, // 199
	"MOUNT_ROOT_TMPFS"sv, // 200
	"CHILD_TAG"sv, // 201
	"CERTIFICATE"sv, // 202
	"UNCACHED"sv, // 203
	"PID_NAMESPACE_NAME"sv, // 204
	"SUBST_YAML_FILE"sv, // 205
	"ALT_HOST"sv, // 206
	"SUBST_ALT_SYNTAX"sv, // 207
	"CACHE_TAG"sv, // 208
	"REQUIRE_CSRF_TOKEN"sv, // 209
	"SEND_CSRF_TOKEN"sv, // 210
	""sv, // 211 - unused
	"REQUEST_URI_VERBATIM"sv, // 212
	"DEFER"sv, // 213
	"STDERR_POND"sv, // 214
	"CHAIN"sv, // 215
	"BREAK_CHAIN"sv, // 216
	"CHAIN_HEADER"sv, // 217
	"FILTER_NO_BODY"sv, // 218
	"HTTP_AUTH"sv, // 219
	"TOKEN_AUTH"sv, // 220
	"AUTH_TOKEN"sv, // 221
	"MOUNT_EMPTY"sv, // 222
	"TINY_IMAGE"sv, // 223
	"ATTACH_SESSION"sv, // 224
	"DISCARD_REALM_SESSION"sv, // 225
	"LIKE_HOST"sv, // 226
	"LAYOUT"sv, // 227
	"RECOVER_SESSION"sv, // 228
	"OPTIONAL"sv, // 229
	"AUTO_BROTLI_PATH"sv, // 230
	"TRANSPARENT_CHAIN"sv, // 231
	"STATS_TAG"sv, // 232
	"MOUNT_DEV"sv, // 233
	"BIND_MOUNT_FILE"sv, // 234
	"EAGER_CACHE"sv, // 235
	"AUTO_FLUSH_CACHE"sv, // 236
	"PARALLELISM"sv, // 237
	"EXPIRES_RELATIVE_WITH_QUERY"sv, // 238
	"CGROUP_XATTR"sv, // 239
	"CHECK_HEADER"sv, // 240
	"PLAN"sv, // 241
	"CHDIR"sv, // 242
	"SESSION_COOKIE_SAME_SITE"sv, // 243
	"NO_PASSWORD"sv, // 244
	"REALM_SESSION"sv, // 245
	"WRITE_FILE"sv, // 246
	"PATH_EXISTS"sv, // 247
	"AUTHORIZED_KEYS"sv, // 248
	"AUTO_BROTLI"sv, // 249
	"DISPOSABLE"sv, // 250
	"DISCARD_QUERY_STRING"sv, // 251
	"MOUNT_NAMED_TMPFS"sv, // 252
	"BENEATH"sv, // 253
	"MAPPED_UID_GID"sv, // 254
	"NO_HOME_AUTHORIZED_KEYS"sv, // 255
	"TIMEOUT"sv, // 256
	"MOUNT_LISTEN_STREAM"sv, // 257
	"ANALYTICS_ID"sv, // 258
	"GENERATOR"sv, // 259
	"IGNORE_NO_CACHE"sv, // 260
	"AUTO_COMPRESS_ONLY_TEXT"sv, // 261
	"REGEX_RAW"sv, // 262
	"ALLOW_REMOTE_NETWORK"sv, // 263
	"RATE_LIMIT_SITE_REQUESTS"sv, // 264
	"ACCEPT_HTTP"sv, // 265
	"CAP_SYS_RESOURCE"sv, // 266
	"CHROOT"sv, // 267
	"TMPFS_DIRS_READABLE"sv, // 268
	"SYMLINK"sv, // 269
	"BIND_MOUNT_RW_EXEC"sv, // 270
	"BIND_MOUNT_FILE_EXEC"sv, // 271
	"REAL_UID_GID"sv, // 272
	"RATE_LIMIT_SITE_TRAFFIC"sv, // 273
	"ARCH"sv, // 274
	"MAPPED_REAL_UID_GID"sv, // 275
	"PEEK"sv, // 276
	"ALLOW_PTRACE"sv, // 277
	"ACCESS_CONTROL_ALLOW_ALL"sv, // 278
};

// Static assertion to verify the array size matches the highest enum value + 1
static_assert(translation_command_names.size() == std::to_underlying(TranslationCommand::ACCESS_CONTROL_ALLOW_ALL) + 1);

// Static assertions to verify specific indexes are placed correctly
static_assert(translation_command_names[std::to_underlying(TranslationCommand::BEGIN)] == "BEGIN"sv);
static_assert(translation_command_names[std::to_underlying(TranslationCommand::END)] == "END"sv);
static_assert(translation_command_names[std::to_underlying(TranslationCommand::HOST)] == "HOST"sv);
static_assert(translation_command_names[std::to_underlying(TranslationCommand::URI)] == "URI"sv);
static_assert(translation_command_names[std::to_underlying(TranslationCommand::WAS)] == "WAS"sv);
static_assert(translation_command_names[std::to_underlying(TranslationCommand::HOME)] == "HOME"sv);
static_assert(translation_command_names[std::to_underlying(TranslationCommand::REALM)] == "REALM"sv);
static_assert(translation_command_names[std::to_underlying(TranslationCommand::ACCESS_CONTROL_ALLOW_ALL)] == "ACCESS_CONTROL_ALLOW_ALL"sv);

std::string_view
ToString(TranslationCommand command) noexcept
{
	const auto index = static_cast<std::size_t>(command);
	if (index >= translation_command_names.size()) {
		return {};
	}
	return translation_command_names[index];
}

TranslationCommand
ParseTranslationCommand(std::string_view name) noexcept
{
	const auto it = std::find(translation_command_names.begin(),
				  translation_command_names.end(),
				  name);

	if (it == translation_command_names.end() || it->empty())
		return {}; // zero-initialized TranslationCommand

	return static_cast<TranslationCommand>(std::distance(translation_command_names.begin(), it));
}
