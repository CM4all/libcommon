/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "RBindSocket.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"

UniqueSocketDescriptor
ResolveBindSocket(const char *host_and_port, int default_port,
                  const struct addrinfo &hints)
{
    const auto ail = Resolve(host_and_port, default_port, &hints);
    const auto &ai = ail.GetBest();

    UniqueSocketDescriptor s;
    if (!s.CreateNonBlock(ai.GetFamily(), ai.GetType(), ai.GetProtocol()))
        throw MakeErrno("Failed to create socket");

    if (!s.Bind(ai))
        throw MakeErrno("Failed to bind");

    return s;
}

static UniqueSocketDescriptor
ResolveBindSocket(const char *host_and_port, int default_port, int socktype)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG|AI_PASSIVE;
    hints.ai_socktype = socktype;

    return ResolveBindSocket(host_and_port, default_port, hints);
}

UniqueSocketDescriptor
ResolveBindStreamSocket(const char *host_and_port, int default_port)
{
    return ResolveBindSocket(host_and_port, default_port, SOCK_STREAM);
}

UniqueSocketDescriptor
ResolveBindDatagramSocket(const char *host_and_port, int default_port)
{
    return ResolveBindSocket(host_and_port, default_port, SOCK_DGRAM);
}
