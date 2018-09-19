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

#include "util/Compiler.h"

class AllocatorPtr;
class MatchInfo;

struct MountList {
	MountList *next;

	const char *source;
	const char *target;

#if TRANSLATION_ENABLE_EXPAND
	bool expand_source;
#endif

	bool writable;

	/**
	 * Omit the MS_NOEXEC flag?
	 */
	bool exec;

	constexpr MountList(const char *_source, const char *_target,
#if !TRANSLATION_ENABLE_EXPAND
			    gcc_unused
#endif
			    bool _expand_source=false, bool _writable=false,
			    bool _exec=false)
		:next(nullptr), source(_source), target(_target),
#if TRANSLATION_ENABLE_EXPAND
		expand_source(_expand_source),
#endif
		writable(_writable), exec(_exec) {
	}

	MountList(AllocatorPtr alloc, const MountList &src);

#if TRANSLATION_ENABLE_EXPAND
	bool IsExpandable() const {
		return expand_source;
	}

	gcc_pure
	static bool IsAnyExpandable(MountList *m) {
		for (; m != nullptr; m = m->next)
			if (m->IsExpandable())
				return true;

		return false;
	}

	void Expand(AllocatorPtr alloc, const MatchInfo &match_info);
	static void ExpandAll(AllocatorPtr alloc, MountList *m,
			      const MatchInfo &match_info);
#endif

	/**
	 * Throws std::system_error on error.
	 */
	void Apply() const;

	static MountList *CloneAll(AllocatorPtr alloc, const MountList *src);

	/**
	 * Throws std::system_error on error.
	 */
	static void ApplyAll(const MountList *m);

	char *MakeId(char *p) const;
	static char *MakeIdAll(char *p, const MountList *m);
};
