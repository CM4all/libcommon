// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
	const std::string path;

	UniqueFileDescriptor fd;

	int remount_flags = 0;

	template<typename P>
	explicit Item(P &&_path) noexcept
		:path(std::forward<P>(_path)) {}

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

	items.emplace_back(std::string_view{});
	items.back().fd = OpenPath(path, O_DIRECTORY);
}

struct VfsBuilder::FindWritableResult {
	const Item *item;
	std::string_view suffix;
};

VfsBuilder::FindWritableResult
VfsBuilder::FindWritable(std::string_view path) const noexcept
{
	for (auto i = items.rbegin(), end = items.rend(); i != end; ++i) {
		auto suffix = path;
		if (!SkipPrefix(suffix, std::string_view{i->path}))
			/* mismatch, continue searching */
			continue;

		if (!suffix.empty()) {
			if (suffix.front() != '/')
				/* mismatch, continue searching */
				continue;

			suffix.remove_prefix(1);
		}

		if (!i->IsWritable())
			/* not writable: stop here */
			break;

		return {&*i, suffix};
	}

	return {nullptr, std::string_view{}};
}

static void
MakeDirs(FileDescriptor fd, std::string_view suffix)
{
	UniqueFileDescriptor ufd;

	std::string name2;

	for (const auto name : IterableSplitString(suffix, '/')) {
		if (name.empty())
			continue;

		if (!name2.empty())
			fd = ufd = OpenPath(fd, name2.c_str(), O_DIRECTORY);

		name2 = name;

		if (mkdirat(fd.Get(), name2.c_str(), 0711) < 0) {
			const int e = errno;
			if (e == EEXIST)
				continue;

			throw FormatErrno(e,
					  "Failed to create mount point %.*s",
					  int(suffix.size()),
					  suffix.data());
		}
	}
}

void
VfsBuilder::Add(std::string_view path)
{
	assert(path.empty() || path.front() == '/');

	const auto fw = FindWritable(path);
	if (fw.item != nullptr) {
		if (fw.suffix.empty())
			throw FormatRuntimeError("Already a mount point: %.*s",
						 int(path.size()),
						 path.data());

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

	item.fd = OpenPath(item.path.c_str(), O_DIRECTORY);
}

void
VfsBuilder::ScheduleRemount(int flags) noexcept
{
	assert(!items.empty());

	auto &item = items.back();
	assert(item.remount_flags == 0);

	item.remount_flags = MS_REMOUNT|flags;
}

bool
VfsBuilder::MakeOptionalDirectory(std::string_view path)
{
	assert(path.empty() || path.front() == '/');

	const auto fw = FindWritable(path);
	if (fw.item == nullptr)
		return false;

	MakeDirs(fw.item->fd, fw.suffix);
	return true;
}

void
VfsBuilder::MakeDirectory(std::string_view path)
{
	if (!MakeOptionalDirectory(path))
		throw FormatRuntimeError("Not writable: %.*s",
					 int(path.size()),
					 path.data());
}

void
VfsBuilder::Finish()
{
	for (const auto &i : items) {
		if (i.remount_flags != 0 &&
		    mount(nullptr, i.path.c_str(), nullptr, i.remount_flags,
			  nullptr) < 0)
			throw FormatErrno("Failed to remount %s",
					  i.path.c_str());
	}
}
