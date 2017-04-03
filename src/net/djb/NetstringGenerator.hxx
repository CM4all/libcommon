/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef NETSTRING_GENERATOR_HXX
#define NETSTRING_GENERATOR_HXX

#include <list>

template<typename T> struct ConstBuffer;

class NetstringGenerator {
    char header_buffer[32];

public:
    /**
     * @param comma generate the trailing comma?
     */
    void operator()(std::list<ConstBuffer<void>> &list, bool comma=true);
};

#endif
