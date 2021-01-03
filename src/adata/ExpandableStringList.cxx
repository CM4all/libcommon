/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "ExpandableStringList.hxx"
#include "AllocatorPtr.hxx"
#include "util/ConstBuffer.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <algorithm>

ExpandableStringList::ExpandableStringList(AllocatorPtr alloc,
					   const ExpandableStringList &src)
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
ExpandableStringList::IsExpandable() const
{
	for (const auto &i : list)
		if (i.expandable)
			return true;

	return false;
}

void
ExpandableStringList::Expand(AllocatorPtr alloc, const MatchInfo &match_info)
{
	for (auto &i : list) {
		if (!i.expandable)
			continue;

		i.value = expand_string_unescaped(alloc, i.value, match_info);
	}
}

#endif

void
ExpandableStringList::Builder::Add(AllocatorPtr alloc,
				   const char *value, bool expandable)
{
	auto *item = last = alloc.New<Item>(value, expandable);
	tail = List::insert_after(tail, *item);
}

ConstBuffer<const char *>
ExpandableStringList::ToArray(AllocatorPtr alloc) const
{
	const size_t n = std::distance(begin(), end());
	const char **p = alloc.NewArray<const char *>(n);
	std::copy(begin(), end(), p);
	return {p, n};
}
