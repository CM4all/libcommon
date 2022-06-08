/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ParamWrapper.hxx"

#include <vector>
#include <cstddef>
#include <cstdio>

namespace Pg {

template<typename T>
struct DynamicParamWrapper {
	ParamWrapper<T> wrapper;

	DynamicParamWrapper(const T &t):wrapper(t) {}

	constexpr static size_t Count(const T &) {
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

	constexpr static size_t Count(const std::vector<T> &v) {
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
