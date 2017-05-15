/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SeccompFilter.hxx"

namespace Seccomp {

Filter::Filter(uint32_t def_action)
    :ctx(seccomp_init(def_action))
{
    if (ctx == nullptr)
        throw std::runtime_error("seccomp_init() failed");
}

void
Filter::Reset(uint32_t def_action)
{
    int error = seccomp_reset(ctx, def_action);
    if (error < 0)
        throw MakeErrno(-error, "seccomp_reset() failed");
}

void
Filter::Load() const
{
    int error = seccomp_load(ctx);
    if (error < 0)
        throw MakeErrno(-error, "seccomp_load() failed");
}

} // namespace Seccomp
