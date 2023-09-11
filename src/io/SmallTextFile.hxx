// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "WithFile.hxx"
#include "system/Error.hxx"
#include "util/IterableSplitString.hxx"

#include <array>
#include <concepts>
#include <string_view>

template<std::size_t buffer_size>
class SmallTextFileBuffer {
	std::array<char, buffer_size> buffer;
	std::size_t fill;

public:
	explicit SmallTextFileBuffer(auto &&file) {
		fill = WithReadOnly(file, [this](auto fd){
			const auto nbytes = fd.ReadAt(0, buffer.data(), buffer.size());
			if (nbytes < 0)
				throw MakeErrno("Failed to read file");

			return static_cast<std::size_t>(nbytes);
		});
	}

	operator std::string_view() const noexcept {
		return {buffer.data(), fill};
	}
};

/**
 * Read the specified file into a small stack buffer and pass it as
 * std::string_view to the given function.
 *
 * This function ignores the current file position (if the file is
 * already open) and always reads from offset 0.
 */
template<std::size_t buffer_size>
decltype(auto)
WithSmallTextFile(auto &&file, std::invocable<std::string_view> auto f)
{
	const SmallTextFileBuffer<buffer_size> buffer{file};
	return f(std::string_view{buffer});
}

/**
 * Read the specified file into a small stack buffer and invoke the
 * given function for each line (with the newline character already
 * stripped, but not other whitespace).
 *
 * This function ignores the current file position (if the file is
 * already open) and always reads from offset 0.
 */
template<std::size_t buffer_size>
decltype(auto)
ForEachTextLine(auto &&file, std::invocable<std::string_view> auto f)
{
	return WithSmallTextFile<buffer_size>(file, [&f](std::string_view contents){
		for (const auto i : IterableSplitString(contents, '\n'))
			f(i);
	});
}
