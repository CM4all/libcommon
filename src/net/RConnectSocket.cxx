/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "RConnectSocket.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"

UniqueSocketDescriptor
ResolveConnectSocket(const char *host_and_port, int default_port,
                     const struct addrinfo &hints,
                     std::chrono::duration<int, std::milli> timeout)
{
    const auto ail = Resolve(host_and_port, default_port, &hints);
    const auto &ai = ail.GetBest();

    UniqueSocketDescriptor s;
    if (!s.CreateNonBlock(ai.GetFamily(), ai.GetType(), ai.GetProtocol()))
        throw MakeErrno("Failed to create socket");

    if (!s.Connect(ai)) {
        if (errno != EINPROGRESS)
            throw MakeErrno("Failed to connect");

        int w = s.WaitWritable(timeout.count());
        if (w < 0)
            throw MakeErrno("Connect wait error");
        else if (w == 0)
            throw std::runtime_error("Connect timeout");

        int err = s.GetError();
        if (err != 0)
            throw MakeErrno(err, "Failed to connect");
    }

    return s;
}

static UniqueSocketDescriptor
ResolveConnectSocket(const char *host_and_port, int default_port,
                     int socktype,
                     std::chrono::duration<int, std::milli> timeout)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_socktype = socktype;

    return ResolveConnectSocket(host_and_port, default_port, hints, timeout);
}

UniqueSocketDescriptor
ResolveConnectStreamSocket(const char *host_and_port, int default_port,
                           std::chrono::duration<int, std::milli> timeout)
{
    return ResolveConnectSocket(host_and_port, default_port, SOCK_STREAM,
                                timeout);
}

UniqueSocketDescriptor
ResolveConnectDatagramSocket(const char *host_and_port, int default_port)
{
    /* hard-coded zero timeout, because "connecting" a datagram socket
       cannot block */

    return ResolveConnectSocket(host_and_port, default_port, SOCK_DGRAM,
                                std::chrono::seconds(0));
}
