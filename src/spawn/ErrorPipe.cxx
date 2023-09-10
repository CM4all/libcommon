// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ErrorPipe.hxx"
#include "io/FileDescriptor.hxx"
#include "io/Iovec.hxx"
#include "util/Exception.hxx"
#include "util/SpanCast.hxx"

#include <array>
#include <stdexcept>

void
WriteErrorPipe(FileDescriptor p, std::string_view prefix,
	       std::string_view msg) noexcept
{
	const std::array v{
		MakeIovec(AsBytes(prefix)),
		MakeIovec(AsBytes(msg)),
	};

	[[maybe_unused]]
	auto nbytes = writev(p.Get(), v.data(), v.size());
}

void
WriteErrorPipe(FileDescriptor p, std::string_view prefix,
	       std::exception_ptr e) noexcept
{
	WriteErrorPipe(p, prefix, GetFullMessage(e));
}

void
ReadErrorPipe(FileDescriptor p)
{
	char buffer[1024];
	ssize_t nbytes = p.Read(buffer, sizeof(buffer) - 1);
	if (nbytes > 0) {
		buffer[nbytes] = 0;
		throw std::runtime_error{buffer};
	}
}
