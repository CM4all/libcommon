// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "WasOutputStream.hxx"

extern "C" {
#include <was/simple.h>
}

void
WasOutputStream::Write(std::span<const std::byte> src)
{
	if (!was_simple_write(w, src.data(), src.size()))
		throw WriteFailed{};
}
