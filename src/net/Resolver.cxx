/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Resolver.hxx"
#include "AddressInfo.hxx"
#include "util/RuntimeError.hxx"

#include <socket/resolver.h>

AddressInfoList
Resolve(const char *host_and_port, int default_port,
        const struct addrinfo *hints)
{
    struct addrinfo *ai;
    int result = socket_resolve_host_port(host_and_port, default_port,
                                          hints, &ai);
    if (result != 0)
        throw FormatRuntimeError("Failed to resolve '%s': %s",
                                 host_and_port, gai_strerror(result));

    return AddressInfoList(ai);
}
