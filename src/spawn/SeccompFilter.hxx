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

/**
 * Reference to a system call argument.
 */
class Arg {
    unsigned arg;

public:
    explicit Arg(unsigned _arg):arg(_arg) {}

    auto Cmp(enum scmp_compare op, scmp_datum_t datum) const {
        return SCMP_CMP(arg, op, datum);
    }

    auto operator==(scmp_datum_t datum) const {
        return Cmp(SCMP_CMP_EQ, datum);
    }

    auto operator!=(scmp_datum_t datum) const {
        return Cmp(SCMP_CMP_EQ, datum);
    }

    auto operator<(scmp_datum_t datum) const {
        return Cmp(SCMP_CMP_LT, datum);
    }

    auto operator>(scmp_datum_t datum) const {
        return Cmp(SCMP_CMP_GT, datum);
    }

    auto operator<=(scmp_datum_t datum) const {
        return Cmp(SCMP_CMP_LE, datum);
    }

    auto operator>=(scmp_datum_t datum) const {
        return Cmp(SCMP_CMP_GE, datum);
    }

    auto operator&(scmp_datum_t mask) const {
        return MaskedArg(arg, mask);
    }

private:
    /**
     * Internal helper class.  Do not use directly.
     */
    class MaskedArg {
        unsigned arg;
        scmp_datum_t mask;

    public:
        MaskedArg(unsigned _arg, scmp_datum_t _mask):arg(_arg), mask(_mask) {}

        auto operator==(scmp_datum_t datum) const {
            return SCMP_CMP(arg, SCMP_CMP_MASKED_EQ, mask, datum);
        }
    };
};

} // namespace Seccomp

#endif

