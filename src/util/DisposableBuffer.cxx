// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "DisposableBuffer.hxx"

#include <algorithm>

DisposableBuffer
DisposableBuffer::Dup(std::string_view src) noexcept
{
	DisposableBuffer b{ToDeleteArray(new char[src.size()]), src.size()};
	std::copy(src.begin(), src.end(), (char *)b.data());
	return b;
}

DisposableBuffer
DisposableBuffer::Dup(std::span<const std::byte> src) noexcept
{
	if (src.data() == nullptr)
		return nullptr;

	DisposableBuffer b{ToDeleteArray(new std::byte[src.size()]), src.size()};
	std::copy(src.begin(), src.end(), (std::byte *)b.data());
	return b;
}
