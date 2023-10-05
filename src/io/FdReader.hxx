// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Reader.hxx"
#include "FileDescriptor.hxx"

class FdReader final : public Reader {
	FileDescriptor fd;

public:
	FdReader(FileDescriptor _fd) noexcept
		:fd(_fd) {}

	FileDescriptor Get() const noexcept {
		return fd;
	}

	/* virtual methods from class Reader */
	std::size_t Read(std::span<std::byte> dest) override;
};
