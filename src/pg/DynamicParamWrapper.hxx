/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PG_DYNAMIC_PARAM_WRAPPER_HXX
#define PG_DYNAMIC_PARAM_WRAPPER_HXX

#include "ParamWrapper.hxx"

#include "util/Compiler.h"

#include <vector>
#include <cstddef>
#include <cstdio>

namespace Pg {

template<typename T>
struct DynamicParamWrapper {
    ParamWrapper<T> wrapper;

    DynamicParamWrapper(const T &t):wrapper(t) {}

    constexpr static size_t Count(gcc_unused const T &t) {
        return 1;
    }

    template<typename O, typename S, typename F>
    unsigned Fill(O output, S size, F format) const {
        *output = wrapper.GetValue();
        *size = wrapper.GetSize();
        *format = wrapper.IsBinary();
        return 1;
    }
};

template<typename T>
struct DynamicParamWrapper<std::vector<T>> {
    std::vector<DynamicParamWrapper<T>> items;

    constexpr DynamicParamWrapper(const std::vector<T> &params)
        :items(params.begin(), params.end()) {}

    constexpr static size_t Count(gcc_unused const std::vector<T> &v) {
        return v.size();
    }

    template<typename O, typename S, typename F>
    unsigned Fill(O output, S size, F format) const {
        unsigned total = 0;
        for (const auto &i : items) {
            const unsigned n = i.Fill(output, size, format);
            std::advance(output, n);
            std::advance(size, n);
            std::advance(format, n);
            total += n;
        }

        return total;
    }
};

} /* namespace Pg */

#endif
