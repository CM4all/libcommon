// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
