/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "UniqueSocketDescriptor.hxx"
#include "StaticSocketAddress.hxx"

#include <sys/socket.h>

UniqueSocketDescriptor
UniqueSocketDescriptor::Accept(StaticSocketAddress &address) const
{
    assert(IsDefined());

    address.size = address.GetCapacity();
    int result = accept4(fd, address, &address.size,
                         SOCK_CLOEXEC|SOCK_NONBLOCK);
    return UniqueSocketDescriptor(result);
}
