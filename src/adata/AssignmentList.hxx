// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/IntrusiveForwardList.hxx"
#include "util/TagStructs.hxx"

#include <string_view>

class AllocatorPtr;

/**
 * A simple list of name/value pairs.
 */
class AssignmentList {
public:
	struct Item : IntrusiveForwardListHook {
		const char *const name;
		const char *const value;

		constexpr Item(const char *_name, const char *_value) noexcept
			:name(_name), value(_value) {}
	};

private:
	IntrusiveForwardList<Item> list;

public:
	constexpr AssignmentList() = default;

	AssignmentList(AllocatorPtr alloc, const AssignmentList &src) noexcept;

	constexpr AssignmentList(ShallowCopy shallow_copy,
				 const AssignmentList &src) noexcept
		:list(shallow_copy, src.list) {}

	AssignmentList(AssignmentList &&) = default;
	AssignmentList &operator=(AssignmentList &&) = default;

	constexpr bool empty() const noexcept {
		return list.empty();
	}

	constexpr auto begin() const noexcept {
		return list.begin();
	}

	constexpr auto end() const noexcept {
		return list.end();
	}

	/**
	 * Copy the strings using the given #AllocatorPtr and insert
	 * them as new items into the list.
	 */
	void Add(AllocatorPtr alloc,
		 std::string_view name, std::string_view value) noexcept;

	/**
	 * Insert an externally allocated #Item into the list.
	 */
	void Add(Item &item) noexcept {
		list.push_front(item);
	}
};
