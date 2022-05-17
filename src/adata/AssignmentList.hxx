/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "util/IntrusiveForwardList.hxx"
#include "util/ShallowCopy.hxx"

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
