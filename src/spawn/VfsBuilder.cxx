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

#include "VfsBuilder.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/IterableSplitString.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"

#include <string>

#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>

struct VfsBuilder::Item {
	const char *path;

	UniqueFileDescriptor fd;

	int remount_flags = 0;

	explicit Item(const char *_path) noexcept
		:path(_path) {}

	bool IsWritable() const noexcept {
		return fd.IsDefined();
	}
};

VfsBuilder::VfsBuilder(uint_least32_t _uid, uint_least32_t _gid) noexcept
	:uid(_uid), gid(_gid) {}

VfsBuilder::~VfsBuilder() noexcept
{
	if (old_umask != -1 && old_umask != 0022)
		umask(old_umask);
}

void
VfsBuilder::AddWritableRoot(const char *path)
{
	assert(items.empty());

	items.emplace_back("");
	items.back().fd = OpenPath(path, O_DIRECTORY);
}

struct VfsBuilder::FindWritableResult {
	const Item *item;
	const char *suffix;
};

VfsBuilder::FindWritableResult
VfsBuilder::FindWritable(const char *path) const
{
	for (auto i = items.rbegin(), end = items.rend(); i != end; ++i) {
		const char *suffix = StringAfterPrefix(path, i->path);
		if (suffix == nullptr)
			/* mismatch, continue searching */
			continue;

		if (*suffix == 0)
			throw FormatRuntimeError("Already a mount point: %s",
						 path);

		if (*suffix != '/')
			/* mismatch, continue searching */
			continue;

		if (!i->IsWritable())
			/* not writable: stop here */
			break;

		++suffix;

		return {&*i, suffix};
	}

	return {nullptr, nullptr};
}

static void
MakeDirs(FileDescriptor fd, const char *suffix)
{
	UniqueFileDescriptor ufd;

	std::string name2;

	for (const auto name : IterableSplitString(suffix, '/')) {
		if (name.empty())
			continue;

		if (!name2.empty())
			fd = ufd = OpenPath(fd, name2.c_str(), O_DIRECTORY);

		name2.assign(name.data, name.size);

		if (mkdirat(fd.Get(), name2.c_str(), 0711) < 0)
			throw FormatErrno("Failed to create mount point %s",
					  suffix);
	}
}

void
VfsBuilder::Add(const char *path)
{
	assert(path != nullptr);
	assert(*path == 0 || *path == '/');

	const auto fw = FindWritable(path);
	if (fw.item != nullptr) {
		if (old_umask == -1)
			old_umask = umask(0022);

		MakeDirs(fw.item->fd, fw.suffix);
	}

	items.emplace_back(path);
}

void
VfsBuilder::MakeWritable()
{
	assert(!items.empty());

	auto &item = items.back();
	assert(!item.fd.IsDefined());

	item.fd = OpenPath(item.path, O_DIRECTORY);
}

void
VfsBuilder::ScheduleRemount(int flags) noexcept
{
	assert(!items.empty());

	auto &item = items.back();
	assert(item.remount_flags == 0);

	item.remount_flags = MS_REMOUNT|flags;
}

void
VfsBuilder::Finish()
{
	for (const auto &i : items) {
		if (i.remount_flags != 0 &&
		    mount(nullptr, i.path, nullptr, i.remount_flags,
			  nullptr) < 0)
		    throw FormatErrno("Failed to remount %s", i.path);
	}
}
