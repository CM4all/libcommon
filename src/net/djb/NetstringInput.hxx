/*
 * A netstring input buffer.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef NETSTRING_INPUT_HXX
#define NETSTRING_INPUT_HXX

#include "util/AllocatedArray.hxx"

#include <cstddef>
#include <cassert>
#include <cstdint>

class NetstringInput {
    enum class State {
        HEADER,
        VALUE,
        FINISHED,
    };

    State state = State::HEADER;

    char header_buffer[32];
    size_t header_position = 0;

    AllocatedArray<uint8_t> value;
    size_t value_position;

    const size_t max_size;

public:
    explicit NetstringInput(size_t _max_size)
        :max_size(_max_size) {}

    enum class Result {
        MORE,
        CLOSED,
        FINISHED,
    };

    /**
     * Throws std::runtime_error on error.
     */
    Result Receive(int fd);

    AllocatedArray<uint8_t> &GetValue() {
        assert(state == State::FINISHED);

        return value;
    }

private:
    Result ReceiveHeader(int fd);
    Result ValueData(size_t nbytes);
    Result ReceiveValue(int fd);
};

#endif
