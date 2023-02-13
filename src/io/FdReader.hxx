// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef FD_READER_HXX
#define FD_READER_HXX

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
	std::size_t Read(void *data, std::size_t size) override;
};

#endif
