/*
 * Copyright 2020-2022 CM4all GmbH
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

#include "WithFile.hxx"
#include "system/Error.hxx"
#include "util/Concepts.hxx"
#include "util/IterableSplitString.hxx"

#include <string_view>

/**
 * Read the specified file into a small stack buffer and pass it as
 * std::string_view to the given function.
 */
template<std::size_t buffer_size>
decltype(auto)
WithSmallTextFile(auto &&file, Invocable<std::string_view> auto f)
{
	char buffer[buffer_size];
	std::size_t n = WithReadOnly(file, [&buffer](auto fd){
		const auto nbytes = fd.Read(buffer, buffer_size);
		if (nbytes < 0)
			throw MakeErrno("Failed to read file");

		return static_cast<std::size_t>(nbytes);
	});

	return f(std::string_view{buffer, n});
}

/**
 * Read the specified file into a small stack buffer and invoke the
 * given function for each line (with the newline character already
 * stripped, but not other whitespace).
 */
template<std::size_t buffer_size>
decltype(auto)
ForEachTextLine(auto &&file, Invocable<std::string_view> auto f)
{
	return WithSmallTextFile<buffer_size>(file, [&f](std::string_view contents){
		for (const auto i : IterableSplitString(contents, '\n'))
			f(i);
	});
}
