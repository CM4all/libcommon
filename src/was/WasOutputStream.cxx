// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "WasOutputStream.hxx"

extern "C" {
#include <was/simple.h>
}

void
WasOutputStream::Write(const void *data, size_t size)
{
	if (!was_simple_write(w, data, size))
		throw WriteFailed{};
}
