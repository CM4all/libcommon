// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct statx;
class UniqueFileDescriptor;

namespace Uring {

class OpenHandler {
public:
	virtual void OnOpen(UniqueFileDescriptor fd) noexcept = 0;

	/**
	 * @param error an errno code
	 */
	virtual void OnOpenError(int error) noexcept = 0;
};

class OpenStatHandler {
public:
	virtual void OnOpenStat(UniqueFileDescriptor fd,
				struct statx &st) noexcept = 0;

	/**
	 * @param error an errno code
	 */
	virtual void OnOpenStatError(int error) noexcept = 0;
};

} // namespace Uring
