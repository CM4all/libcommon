/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "MountList.hxx"
#include "system/BindMount.hxx"
#include "AllocatorPtr.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <sys/mount.h>
#include <string.h>

inline
MountList::MountList(AllocatorPtr alloc, const MountList &src) noexcept
	:source(alloc.Dup(src.source)),
	 target(alloc.Dup(src.target)),
#if TRANSLATION_ENABLE_EXPAND
	 expand_source(src.expand_source),
#endif
	 writable(src.writable),
	 exec(src.exec) {}

MountList *
MountList::CloneAll(AllocatorPtr alloc, const MountList *src) noexcept
{
	MountList *head = nullptr, **tail = &head;

	for (; src != nullptr; src = src->next) {
		MountList *dest = alloc.New<MountList>(alloc, *src);
		*tail = dest;
		tail = &dest->next;
	}

	return head;
}

#if TRANSLATION_ENABLE_EXPAND

void
MountList::Expand(AllocatorPtr alloc, const MatchInfo &match_info)
{
	if (expand_source) {
		expand_source = false;

		source = expand_string_unescaped(alloc, source, match_info);
	}
}

void
MountList::ExpandAll(AllocatorPtr alloc, MountList *m,
		     const MatchInfo &match_info)
{
	for (; m != nullptr; m = m->next)
		m->Expand(alloc, match_info);
}

#endif

inline void
MountList::Apply() const
{
	int flags = MS_NOSUID|MS_NODEV;
	if (!writable)
		flags |= MS_RDONLY;
	if (!exec)
		flags |= MS_NOEXEC;

	BindMount(source, target, flags);
}

void
MountList::ApplyAll(const MountList *m)
{
	for (; m != nullptr; m = m->next)
		m->Apply();
}

char *
MountList::MakeId(char *p) const noexcept
{
	*p++ = ';';
	*p++ = 'm';

	if (writable)
		*p++ = 'w';

	if (exec)
		*p++ = 'x';

	*p++ = ':';
	p = stpcpy(p, source);
	*p++ = '>';
	p = stpcpy(p, target);

	return p;
}

char *
MountList::MakeIdAll(char *p, const MountList *m) noexcept
{
	for (; m != nullptr; m = m->next)
		p = m->MakeId(p);

	return p;
}
