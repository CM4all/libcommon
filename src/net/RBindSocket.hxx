/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef RESOLVE_BIND_SOCKET_HXX
#define RESOLVE_BIND_SOCKET_HXX

struct addrinfo;
class UniqueSocketDescriptor;

/**
 * Resolve a host name and bind to the first resulting address.
 *
 * Throws std::runtime_error on error.
 *
 * @return the socket (non-blocking)
 */
UniqueSocketDescriptor
ResolveBindSocket(const char *host_and_port, int default_port,
                  const struct addrinfo &hints);

UniqueSocketDescriptor
ResolveBindStreamSocket(const char *host_and_port, int default_port);

UniqueSocketDescriptor
ResolveBindDatagramSocket(const char *host_and_port, int default_port);

#endif
