// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef FD_OUTPUT_STREAM_HXX
#define FD_OUTPUT_STREAM_HXX

#include "OutputStream.hxx"
#include "FileDescriptor.hxx"

class FdOutputStream final : public OutputStream {
	FileDescriptor fd;

public:
	explicit FdOutputStream(FileDescriptor _fd):fd(_fd) {}

	FileDescriptor GetFileDescriptor() const noexcept {
		return fd;
	}

	void Write(std::span<const std::byte> src) override;
};

#endif
