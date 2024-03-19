// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UniqueFileDescriptor.hxx"

#include <forward_list>

/**
 * A class that "holds" #FileDescriptor instances and closes them
 * automatically upon destruction.
 */
class FdHolder {
	std::forward_list<UniqueFileDescriptor> list;

public:
	FileDescriptor Insert(UniqueFileDescriptor &&fd) noexcept {
		list.emplace_front(std::move(fd));
		return list.front();
	}
};
