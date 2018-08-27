/*
 * Copyright 2007-2018 Content Management AG
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
#include "util/ShallowCopy.hxx"

#include "util/Compiler.h"

#include <iterator>

class AllocatorPtr;
class MatchInfo;
template<typename T> struct ConstBuffer;

class ExpandableStringList final {
	struct Item {
		Item *next = nullptr;

		const char *value;

#if TRANSLATION_ENABLE_EXPAND
		bool expandable;

		Item(const char *_value, bool _expandable)
			:value(_value), expandable(_expandable) {}
#else
		Item(const char *_value, bool)
			:value(_value) {}
#endif
	};

	Item *head = nullptr;

public:
	ExpandableStringList() = default;
	ExpandableStringList(ExpandableStringList &&) = default;
	ExpandableStringList &operator=(ExpandableStringList &&src) = default;

	constexpr ExpandableStringList(ShallowCopy,
				       const ExpandableStringList &src)
	:head(src.head) {}

	ExpandableStringList(AllocatorPtr alloc, const ExpandableStringList &src);

	gcc_pure
	bool IsEmpty() const {
		return head == nullptr;
	}

	class const_iterator final
		: public std::iterator<std::forward_iterator_tag, const char *> {

		const Item *cursor;

	public:
		const_iterator(const Item *_cursor):cursor(_cursor) {}

		bool operator!=(const const_iterator &other) const {
			return cursor != other.cursor;
		}

		const char *operator*() const {
			return cursor->value;
		}

		const_iterator &operator++() {
			cursor = cursor->next;
			return *this;
		}
	};

	const_iterator begin() const {
		return {head};
	}

	const_iterator end() const {
		return {nullptr};
	}

#if TRANSLATION_ENABLE_EXPAND
	gcc_pure
	bool IsExpandable() const;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Expand(AllocatorPtr alloc, const MatchInfo &match_info);
#endif

	class Builder final {
		Item **tail_r;

		Item *last;

	public:
		Builder() = default;

		Builder(ExpandableStringList &_list)
			:tail_r(&_list.head), last(nullptr) {}

		/**
		 * Add a new item to the end of the list.  The allocator is only
		 * used to allocate the item structure, it does not copy the
		 * string.
		 */
		void Add(AllocatorPtr alloc, const char *value, bool expandable);

#if TRANSLATION_ENABLE_EXPAND
		bool CanSetExpand() const {
			return last != nullptr && !last->expandable;
		}

		void SetExpand(const char *value) const {
			last->value = value;
			last->expandable = true;
		}
#endif
	};

	ConstBuffer<const char *> ToArray(AllocatorPtr alloc) const;
};
