// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketPair.hxx"
#include "SocketError.hxx"

static inline std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
_CreateSocketPair(int domain, int type, int protocol)
{
	int sv[2];
	if (socketpair(domain, type, protocol, sv))
		throw MakeSocketError("socketpair() failed");

	return {
		UniqueSocketDescriptor{sv[0]},
		UniqueSocketDescriptor{sv[1]},
	};
}

std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
CreateSocketPair(int domain, int type, int protocol)
{
#ifdef SOCK_CLOEXEC
	/* implemented since Linux 2.6.27 */
	type |= SOCK_CLOEXEC;
#endif

	return _CreateSocketPair(domain, type, protocol);
}

std::pair<UniqueSocketDescriptor, UniqueSocketDescriptor>
CreateSocketPairNonBlock(int domain, int type, int protocol)
{
#ifdef SOCK_NONBLOCK
	type |= SOCK_NONBLOCK;
#endif

	return CreateSocketPair(domain, type, protocol);
}
