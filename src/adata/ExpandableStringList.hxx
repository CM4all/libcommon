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

#include "translation/Features.hxx"
#include "util/IntrusiveForwardList.hxx"
#include "util/ShallowCopy.hxx"

#include <iterator>
#include <span>

class AllocatorPtr;
class MatchData;

class ExpandableStringList final {
	struct Item : IntrusiveForwardListHook {
		const char *value;

#if TRANSLATION_ENABLE_EXPAND
		bool expandable;

		Item(const char *_value, bool _expandable) noexcept
			:value(_value), expandable(_expandable) {}
#else
		Item(const char *_value, bool) noexcept
			:value(_value) {}
#endif
	};

	using List = IntrusiveForwardList<Item>;

	List list;

public:
	ExpandableStringList() noexcept = default;
	ExpandableStringList(ExpandableStringList &&) noexcept = default;
	ExpandableStringList &operator=(ExpandableStringList &&src) noexcept = default;

	constexpr ExpandableStringList(ShallowCopy shallow_copy,
				       const ExpandableStringList &src) noexcept
		:list(shallow_copy, src.list) {}

	ExpandableStringList(AllocatorPtr alloc, const ExpandableStringList &src) noexcept;

	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return list.empty();
	}

	class const_iterator final {
		List::const_iterator i;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = const char *;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using reference = value_type &;

		const_iterator(List::const_iterator _i) noexcept
			:i(_i) {}

		bool operator!=(const const_iterator &other) const noexcept {
			return i != other.i;
		}

		const char *operator*() const noexcept {
			return i->value;
		}

		const_iterator &operator++() noexcept {
			++i;
			return *this;
		}
	};

	auto begin() const noexcept {
		return const_iterator{list.begin()};
	}

	auto end() const noexcept {
		return const_iterator{list.end()};
	}

#if TRANSLATION_ENABLE_EXPAND
	[[gnu::pure]]
	bool IsExpandable() const noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Expand(AllocatorPtr alloc, const MatchData &match_data) noexcept;
#endif

	class Builder final {
		List::iterator tail;

		Item *last;

	public:
		Builder() noexcept = default;

		Builder(ExpandableStringList &_list) noexcept
			:tail(_list.list.before_begin()), last(nullptr) {}

		/**
		 * Add a new item to the end of the list.  The allocator is only
		 * used to allocate the item structure, it does not copy the
		 * string.
		 */
		void Add(AllocatorPtr alloc, const char *value,
			 bool expandable) noexcept;

#if TRANSLATION_ENABLE_EXPAND
		bool CanSetExpand() const noexcept {
			return last != nullptr && !last->expandable;
		}

		void SetExpand(const char *value) const noexcept {
			last->value = value;
			last->expandable = true;
		}
#endif
	};

	std::span<const char *const> ToArray(AllocatorPtr alloc) const noexcept;
};
