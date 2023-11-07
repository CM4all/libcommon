// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "UniqueSocketDescriptor.hxx"

#include <utility>

#include <sys/socket.h>

class UniqueSocketDescriptor;

/**
 * Wrapper for socketpair().
 *
 * Throws on error.
 */
std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
CreateSocketPair(int domain, int type, int protocol=0);

/**
 * Shortcut for CreateSocketPair(AF_LOCAL, type).
 */
static std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
CreateSocketPair(int type)
{
	return CreateSocketPair(AF_LOCAL, type);
}

/**
 * Like CreateSocketPair(), but with SOCK_NONBLOCK (if available).
 */
std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
CreateSocketPairNonBlock(int domain, int type, int protocol=0);

/**
 * Shortcut for CreateSocketPairNonBlock(AF_LOCAL, type).
 */
static std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
CreateSocketPairNonBlock(int type)
{
	return CreateSocketPairNonBlock(AF_LOCAL, type);
}

/**
 * Shortcut for CreateSocketPair(SOCK_STREAM).
 */
static inline std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
CreateStreamSocketPair()
{
	return CreateSocketPair(SOCK_STREAM);
}

/**
 * Shortcut for CreateSocketPairNonBlock(SOCK_STREAM).
 */
static inline std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
CreateStreamSocketPairNonBlock()
{
	return CreateSocketPairNonBlock(SOCK_STREAM);
}
