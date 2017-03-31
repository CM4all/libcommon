/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "ResourceLimits.hxx"
#include "system/Error.hxx"
#include "util/djbhash.h"

#include <assert.h>
#include <stdio.h>

inline void
ResourceLimit::Get(int resource)
{
    if (getrlimit(resource, this) < 0)
        throw FormatErrno("getrlimit(%d) failed", resource);
}

inline void
ResourceLimit::Set(int resource) const
{
    if (setrlimit(resource, this) < 0)
        throw FormatErrno("setrlimit(%d, %lu, %lu) failed",
                          resource, (unsigned long)rlim_cur,
                          (unsigned long)rlim_max);
}

inline void
ResourceLimit::OverrideFrom(const ResourceLimit &src)
{
    if (src.rlim_cur != UNDEFINED)
        rlim_cur = src.rlim_cur;

    if (src.rlim_max != UNDEFINED)
        rlim_max = src.rlim_max;
}

inline void
ResourceLimit::CompleteFrom(int resource, const ResourceLimit &src)
{
    Get(resource);
    OverrideFrom(src);
}

gcc_pure
inline bool
ResourceLimits::IsEmpty() const
{
    for (const auto &i : values)
        if (!i.IsEmpty())
            return false;

    return true;
}

gcc_pure
inline unsigned
ResourceLimits::GetHash() const
{
    return djb_hash(this, sizeof(*this));
}

char *
ResourceLimits::MakeId(char *p) const
{
    if (IsEmpty())
        return p;

    *p++ = ';';
    *p++ = 'r';
    p += sprintf(p, "%08x", GetHash());
    return p;
}

/**
 * Replace ResourceLimit::UNDEFINED with current values.
 */
static const ResourceLimit &
complete_rlimit(int resource, const ResourceLimit &r, ResourceLimit &buffer)
{
    if (r.IsFull())
        /* already complete */
        return r;

    buffer.CompleteFrom(resource, r);
    return buffer;
}

static void
rlimit_apply(int resource, const ResourceLimit &r)
{
    if (r.IsEmpty())
        return;

    ResourceLimit buffer;
    const auto &r2 = complete_rlimit(resource, r, buffer);
    r2.Set(resource);
}

void
ResourceLimits::Apply() const
{
    for (unsigned i = 0; i < RLIM_NLIMITS; ++i)
        rlimit_apply(i, values[i]);
}

bool
ResourceLimits::Parse(const char *s)
{
    enum {
        BOTH,
        SOFT,
        HARD,
    } which = BOTH;

    char ch;
    while ((ch = *s++) != 0) {
        unsigned resource;

        switch (ch) {
        case 'S':
            which = SOFT;
            continue;

        case 'H':
            which = HARD;
            continue;

        case 't':
            resource = RLIMIT_CPU;
            break;

        case 'f':
            resource = RLIMIT_FSIZE;
            break;

        case 'd':
            resource = RLIMIT_DATA;
            break;

        case 's':
            resource = RLIMIT_STACK;
            break;

        case 'c':
            resource = RLIMIT_CORE;
            break;

        case 'm':
            resource = RLIMIT_RSS;
            break;

        case 'u':
            resource = RLIMIT_NPROC;
            break;

        case 'n':
            resource = RLIMIT_NOFILE;
            break;

        case 'l':
            resource = RLIMIT_MEMLOCK;
            break;

        case 'v':
            resource = RLIMIT_AS;
            break;

            /* obsolete:
        case 'x':
            resource = RLIMIT_LOCKS;
            break;
            */

        case 'i':
            resource = RLIMIT_SIGPENDING;
            break;

        case 'q':
            resource = RLIMIT_MSGQUEUE;
            break;

        case 'e':
            resource = RLIMIT_NICE;
            break;

        case 'r':
            resource = RLIMIT_RTPRIO;
            break;

            /* not supported by bash's "ulimit" command
        case ?:
            resource = RLIMIT_RTTIME;
            break;
            */

        default:
            return false;
        }

        assert(resource < RLIM_NLIMITS);
        struct rlimit *const t = &values[resource];

        unsigned long value;

        if (*s == '!') {
            value = (unsigned long)RLIM_INFINITY;
            ++s;
        } else {
            char *endptr;
            value = strtoul(s, &endptr, 10);
            if (endptr == s)
                return false;

            s = endptr;

            switch (*s) {
            case 'G' : value <<= 10; /* fall through */
            case 'M' : value <<= 10;
            case 'K' : value <<= 10;
                ++s;
            }
        }

        switch (which) {
        case BOTH:
            t->rlim_cur = t->rlim_max = value;
            break;

        case SOFT:
            t->rlim_cur = value;
            break;

        case HARD:
            t->rlim_max = value;
            break;
        }
    }

    return true;
}
