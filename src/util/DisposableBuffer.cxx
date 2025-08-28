// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "DisposableBuffer.hxx"
#include "AllocatedString.hxx"

#include <algorithm>

#include <string.h>

DisposableBuffer::DisposableBuffer(AllocatedString &&src) noexcept
	:data_(ToDeleteArray(src.Steal())),
	 size_(data_ ? strlen(reinterpret_cast<char *>(data_.get())) : 0)
{
}

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
