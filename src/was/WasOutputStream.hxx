// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/OutputStream.hxx"

class WasOutputStream final : public OutputStream {
	struct was_simple *const w;

public:
	explicit WasOutputStream(struct was_simple *_w) noexcept
		:w(_w) {}

	struct WriteFailed {};

	/**
	 * Throws #WriteFailed if was_simple_write() fails.
	 */
	void Write(std::span<const std::byte> src) override;
};
