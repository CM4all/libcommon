// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct TranslateTokenBucketParams {
	float rate, burst;

	void Clear() noexcept {
		rate = -1;
	}

	bool IsDefined() const noexcept {
		return rate > 0;
	}

	bool IsValid() const noexcept {
		// TODO check std::isfinite()?
		return rate > 0 && burst > 0;
	}
};
