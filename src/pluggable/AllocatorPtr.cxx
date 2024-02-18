// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AllocatorPtr.hxx"

#include <algorithm>

std::span<const std::byte>
AllocatorPtr::Dup(std::span<const std::byte> src) const noexcept
{
	if (src.data() == nullptr)
		return {};

	if (src.empty())
		return {(const std::byte *)"", 0};

	return {(const std::byte *)Dup(src.data(), src.size()), src.size()};
}
