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

#pragma once

#include "translation/Features.hxx"
#include "util/IntrusiveForwardList.hxx"

#include <cstdint>

class AllocatorPtr;
class MatchInfo;
class VfsBuilder;

struct Mount : IntrusiveForwardListHook {
	const char *source;
	const char *target;

	enum class Type : uint8_t {
		BIND,
		TMPFS,
	} type = Type::BIND;

#if TRANSLATION_ENABLE_EXPAND
	bool expand_source = false;
#endif

	bool writable;

	/**
	 * Omit the MS_NOEXEC flag?
	 */
	bool exec;

	constexpr Mount(const char *_source, const char *_target,
			bool _writable=false,
			bool _exec=false) noexcept
		:source(_source), target(_target),
		writable(_writable), exec(_exec) {
	}

	struct Tmpfs {};

	constexpr Mount(Tmpfs, const char *_target, bool _writable) noexcept
		:source(nullptr), target(_target),
		 type(Type::TMPFS),
		 writable(_writable), exec(false) {}

	Mount(AllocatorPtr alloc, const Mount &src) noexcept;

#if TRANSLATION_ENABLE_EXPAND
	bool IsExpandable() const noexcept {
		return expand_source;
	}

	[[gnu::pure]]
	static bool IsAnyExpandable(const IntrusiveForwardList<Mount> &list) noexcept {
		for (const auto &i : list)
			if (i.IsExpandable())
				return true;

		return false;
	}

	void Expand(AllocatorPtr alloc, const MatchInfo &match_info);
	static void ExpandAll(AllocatorPtr alloc,
			      IntrusiveForwardList<Mount> &list,
			      const MatchInfo &match_info);
#endif

private:
	void ApplyBindMount(VfsBuilder &vfs_builder) const;
	void ApplyTmpfs(VfsBuilder &vfs_builder) const;

public:
	/**
	 * Throws std::system_error on error.
	 */
	void Apply(VfsBuilder &vfs_builder) const;

	static IntrusiveForwardList<Mount> CloneAll(AllocatorPtr alloc,
						    const IntrusiveForwardList<Mount> &src) noexcept;

	/**
	 * Throws std::system_error on error.
	 */
	static void ApplyAll(const IntrusiveForwardList<Mount> &m,
			     VfsBuilder &vfs_builder);

	char *MakeId(char *p) const noexcept;
	static char *MakeIdAll(char *p, const IntrusiveForwardList<Mount> &m) noexcept;
};
