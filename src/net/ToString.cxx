/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "ToString.hxx"
#include "SocketAddress.hxx"

#include <algorithm>

#include <assert.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdint.h>

static SocketAddress
ipv64_normalize_mapped(SocketAddress address)
{
    const auto &a6 = *(const struct sockaddr_in6 *)(const void *)address.GetAddress();

    if (!address.IsV4Mapped())
        return address;

    uint16_t port;

    struct in_addr inaddr;
    memcpy(&inaddr, ((const char *)&a6.sin6_addr) + 12, sizeof(inaddr));
    port = a6.sin6_port;

    static struct sockaddr_in a4;
    a4.sin_family = AF_INET;
    memcpy(&a4.sin_addr, &inaddr, sizeof(inaddr));
    a4.sin_port = (in_port_t)port;

    return {(const struct sockaddr *)&a4, sizeof(a4)};
}

static bool
LocalToString(char *buffer, size_t buffer_size,
              const struct sockaddr_un *sun, size_t length)
{
    const size_t prefix = (size_t)((struct sockaddr_un *)nullptr)->sun_path;
    assert(length >= prefix);
    length -= prefix;
    if (length >= buffer_size)
        length = buffer_size - 1;

    memcpy(buffer, sun->sun_path, length);
    char *end = buffer + length;

    if (end > buffer && buffer[0] != '\0' && end[-1] == '\0')
        /* don't convert the null terminator of a non-abstract socket
           to a '@' */
        --end;

    /* replace all null bytes with '@'; this also handles abstract
       addresses */
    std::replace(buffer, end, '\0', '@');
    *end = 0;

    return true;
}

bool
ToString(char *buffer, size_t buffer_size,
         SocketAddress address)
{
    if (address.IsNull())
        return false;

    if (address.GetFamily() == AF_LOCAL)
        return LocalToString(buffer, buffer_size,
                             (const struct sockaddr_un *)address.GetAddress(),
                             address.GetSize());

    address = ipv64_normalize_mapped(address);

    char serv[16];
    int ret = getnameinfo(address.GetAddress(), address.GetSize(),
                          buffer, buffer_size,
                          serv, sizeof(serv),
                          NI_NUMERICHOST | NI_NUMERICSERV);
    if (ret != 0)
        return false;

    if (serv[0] != 0) {
        if (address.GetFamily() == AF_INET6) {
            /* enclose IPv6 address in square brackets */

            size_t length = strlen(buffer);
            if (length + 4 >= buffer_size)
                /* no more room */
                return false;

            memmove(buffer + 1, buffer, length);
            buffer[0] = '[';
            buffer[++length] = ']';
            buffer[++length] = 0;
        }

        if (strlen(buffer) + 1 + strlen(serv) >= buffer_size)
            /* no more room */
            return false;

        strcat(buffer, ":");
        strcat(buffer, serv);
    }

    return true;
}

bool
HostToString(char *buffer, size_t buffer_size,
             SocketAddress address)
{
    if (address.IsNull())
        return false;

    if (address.GetFamily() == AF_LOCAL)
        return LocalToString(buffer, buffer_size,
                             (const struct sockaddr_un *)address.GetAddress(),
                             address.GetSize());

    address = ipv64_normalize_mapped(address);

    return getnameinfo(address.GetAddress(), address.GetSize(),
                       buffer, buffer_size,
                       nullptr, 0,
                       NI_NUMERICHOST | NI_NUMERICSERV) == 0;
}
