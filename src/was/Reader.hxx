// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/Reader.hxx"

/**
 * #Reader implementation which reads from the WAS request body.
 */
class WasReader final : public Reader {
	struct was_simple *const w;

public:
	explicit WasReader(struct was_simple *_w) noexcept
		:w(_w) {}

	struct ReadFailed {};

	/**
	 * Throws #ReadFailed if was_simple_read() returns -2.
	 */
	std::size_t Read(void *data, std::size_t size) override;
};
