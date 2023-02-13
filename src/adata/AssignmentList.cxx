// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AssignmentList.hxx"
#include "AllocatorPtr.hxx"

AssignmentList::AssignmentList(AllocatorPtr alloc,
			       const AssignmentList &src) noexcept
{
	auto tail = list.before_begin();

	for (const auto &i : src.list) {
		auto *item = alloc.New<Item>(alloc.Dup(i.name),
					     alloc.Dup(i.value));
		tail = list.insert_after(tail, *item);
	}
}

void
AssignmentList::Add(AllocatorPtr alloc,
		    std::string_view _name, std::string_view _value) noexcept
{
	Add(*alloc.New<Item>(alloc.DupZ(_name), alloc.DupZ(_value)));
}
