// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ExpandableStringList.hxx"
#include "AllocatorPtr.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <algorithm>

ExpandableStringList::ExpandableStringList(AllocatorPtr alloc,
					   const ExpandableStringList &src) noexcept
{
	Builder builder(*this);

	for (const auto &i : src.list)
		builder.Add(alloc, alloc.Dup(i.value),
#if TRANSLATION_ENABLE_EXPAND
			    i.expandable
#else
			    false
#endif
			    );
}

#if TRANSLATION_ENABLE_EXPAND

bool
ExpandableStringList::IsExpandable() const noexcept
{
	for (const auto &i : list)
		if (i.expandable)
			return true;

	return false;
}

void
ExpandableStringList::Expand(AllocatorPtr alloc, const MatchData &match_data) noexcept
{
	for (auto &i : list) {
		if (!i.expandable)
			continue;

		i.value = expand_string_unescaped(alloc, i.value, match_data);
	}
}

#endif

void
ExpandableStringList::Builder::Add(AllocatorPtr alloc,
				   const char *value, bool expandable) noexcept
{
	auto *item = last = alloc.New<Item>(value, expandable);
	tail = List::insert_after(tail, *item);
}

std::span<const char *const>
ExpandableStringList::ToArray(AllocatorPtr alloc) const noexcept
{
	const size_t n = std::distance(begin(), end());
	const char **p = alloc.NewArray<const char *>(n);
	std::copy(begin(), end(), p);
	return {p, n};
}
