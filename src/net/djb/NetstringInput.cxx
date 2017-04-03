/*
 * A netstring input buffer.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "NetstringInput.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
#include "util/CharUtil.hxx"

#include <unistd.h>
#include <string.h>
#include <errno.h>

static bool
OnlyDigits(const char *p, size_t size)
{
    for (size_t i = 0; i != size; ++i)
        if (!IsDigitASCII(p[i]))
            return false;

    return true;
}

inline NetstringInput::Result
NetstringInput::ReceiveHeader(int fd)
{
    ssize_t nbytes = read(fd, header_buffer + header_position,
                          sizeof(header_buffer) - header_position);
    if (nbytes < 0) {
        switch (errno) {
        case EAGAIN:
        case EINTR:
            return Result::MORE;

        case ECONNRESET:
            return Result::CLOSED;

        default:
            throw MakeErrno("read() failed");
        }
    }

    if (nbytes == 0)
        return Result::CLOSED;

    header_position += nbytes;

    char *colon = (char *)memchr(header_buffer + 1, ':', header_position - 1);
    if (colon == nullptr) {
        if (header_position == sizeof(header_buffer) ||
            !OnlyDigits(header_buffer, header_position))
            throw std::runtime_error("Malformed netstring");

        return Result::MORE;
    }

    *colon = 0;
    char *endptr;
    size_t size = strtoul(header_buffer, &endptr, 10);
    if (endptr != colon)
        throw std::runtime_error("Malformed netstring");

    if (size >= max_size)
        throw FormatRuntimeError("Netstring is too large: %zu", size);

    /* allocate only extra byte for the trailing comma */
    value.ResizeDiscard(size + 1);
    state = State::VALUE;
    value_position = 0;

    size_t vbytes = header_position - (colon - header_buffer) - 1;
    memcpy(&value.front(), colon + 1, vbytes);
    return ValueData(vbytes);
}

NetstringInput::Result
NetstringInput::ValueData(size_t nbytes)
{
    assert(state == State::VALUE);

    value_position += nbytes;

    if (value_position >= value.size()) {
        if (value.back() != ',')
            throw std::runtime_error("Malformed netstring");

        /* erase the trailing comma */
        value.SetSize(value.size() - 1);

        state = State::FINISHED;
        return Result::FINISHED;
    }

    return Result::MORE;
}

inline NetstringInput::Result
NetstringInput::ReceiveValue(int fd)
{
    ssize_t nbytes = read(fd, &value.front() + value_position,
                          value.size() - value_position);
    if (nbytes < 0) {
        switch (errno) {
        case EAGAIN:
        case EINTR:
            return Result::MORE;

        case ECONNRESET:
            return Result::CLOSED;

        default:
            throw MakeErrno("read() failed");
        }
    }

    if (nbytes == 0)
        return Result::CLOSED;

    return ValueData(nbytes);
}

NetstringInput::Result
NetstringInput::Receive(int fd)
{
    switch (state) {
    case State::FINISHED:
        assert(false);
        gcc_unreachable();

    case State::HEADER:
        return ReceiveHeader(fd);

    case State::VALUE:
        return ReceiveValue(fd);
    }

    gcc_unreachable();
}
