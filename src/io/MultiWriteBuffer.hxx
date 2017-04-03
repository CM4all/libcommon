/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MULTI_WRITE_BUFFER_HXX
#define MULTI_WRITE_BUFFER_HXX

#include "WriteBuffer.hxx"

#include <array>
#include <cstddef>
#include <cassert>

class MultiWriteBuffer {
    static constexpr size_t MAX_BUFFERS = 8;

    unsigned i = 0, n = 0;

    std::array<WriteBuffer, MAX_BUFFERS> buffers;

public:
    typedef WriteBuffer::Result Result;

    void Push(const void *buffer, size_t size) {
        assert(n < buffers.size());

        buffers[n++] = WriteBuffer(buffer, size);
    }

    /**
     * Throws std::system_error on error.
     */
    Result Write(int fd);
};

#endif
