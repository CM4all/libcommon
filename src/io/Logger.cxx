/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Logger.hxx"
#include "util/StaticArray.hxx"
#include "util/Exception.hxx"

#include <sys/uio.h>
#include <unistd.h>
#include <stdarg.h>

unsigned LoggerDetail::min_level = 1;

LoggerDetail::ParamWrapper<std::exception_ptr>::ParamWrapper(std::exception_ptr ep)
    :ParamWrapper<std::string>(GetFullMessage(ep)) {}

static constexpr struct iovec
ToIovec(StringView s)
{
    return {const_cast<char *>(s.data), s.size};
}

gcc_pure
static struct iovec
ToIovec(const char *s)
{
    return ToIovec(StringView(s));
}

void
LoggerDetail::WriteV(StringView domain,
                     ConstBuffer<StringView> buffers) noexcept
{
    StaticArray<struct iovec, 64> v;

    if (!domain.IsEmpty()) {
        v.push_back(ToIovec("["));
        v.push_back(ToIovec(domain));
        v.push_back(ToIovec("] "));
    }

    for (const auto i : buffers) {
        if (v.size() >= v.capacity() - 1)
            break;

        v.push_back(ToIovec(i));
    }

    v.push_back(ToIovec("\n"));

    writev(STDERR_FILENO, v.raw(), v.size());
}

void
LoggerDetail::Format(unsigned level, StringView domain,
                     const char *fmt, ...) noexcept
{
    if (!CheckLevel(level))
        return;

    char buffer[2048];

    va_list ap;
    va_start(ap, fmt);
    StringView s(buffer, vsnprintf(buffer, sizeof(buffer), fmt, ap));
    va_end(ap);

    WriteV(domain, {&s, 1});
}

