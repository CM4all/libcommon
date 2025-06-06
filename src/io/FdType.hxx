// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

enum FdType {
	/**
	 * No file descriptor available.  Special value that is only
	 * supported by few libraries.
	 */
	FD_NONE = 00,

	FD_FILE = 01,
	FD_PIPE = 02,
	FD_SOCKET = 04,
	FD_TCP = 010,

	/** a character device, such as /dev/zero or /dev/null */
	FD_CHARDEV = 020,
};

typedef unsigned FdTypeMask;

static constexpr FdTypeMask FD_ANY_SOCKET = FD_SOCKET | FD_TCP;
static constexpr FdTypeMask FD_ANY = FD_FILE | FD_PIPE | FD_ANY_SOCKET;

constexpr bool
IsAnySocket(FdType type) noexcept
{
	return (FdTypeMask(type) & FD_ANY_SOCKET) != 0;
}
