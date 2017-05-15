/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SECCOMP_FILTER_HXX
#define SECCOMP_FILTER_HXX

#include "system/Error.hxx"

#include "seccomp.h"

#include <stdexcept>

namespace Seccomp {

class Filter {
    const scmp_filter_ctx ctx;

public:
    /**
     * Throws std::runtime_error on error.
     */
    explicit Filter(uint32_t def_action);

    ~Filter() {
        seccomp_release(ctx);
    }

    Filter(const Filter &) = delete;
    Filter &operator=(const Filter &) = delete;

    /**
     * Throws std::runtime_error on error.
     */
    void Reset(uint32_t def_action);

    void Load() const;

    template<typename... Args>
    void AddRule(uint32_t action, int syscall, Args... args) {
        int error = seccomp_rule_add(ctx, action, syscall, sizeof...(args),
                                     std::forward<Args>(args)...);
        if (error < 0)
            throw FormatErrno(-error, "seccomp_rule_add(%d) failed", syscall);
    }
};

} // namespace Seccomp

#endif

