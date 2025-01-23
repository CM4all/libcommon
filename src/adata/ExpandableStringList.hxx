// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "translation/Features.hxx"
#include "util/IntrusiveForwardList.hxx"
#include "util/TagStructs.hxx"

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
