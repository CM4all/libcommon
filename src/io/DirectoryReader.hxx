// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <dirent.h>

class FileDescriptor;
class UniqueFileDescriptor;

class DirectoryReader {
	DIR *const dir;

public:
	explicit DirectoryReader(const char *path);
	explicit DirectoryReader(UniqueFileDescriptor &&fd);

	DirectoryReader(const DirectoryReader &) = delete;

	~DirectoryReader() noexcept {
		closedir(dir);
	}

	DirectoryReader &operator=(const DirectoryReader &) = delete;

	const char *Read() noexcept {
		const auto *ent = readdir(dir);
		return ent != nullptr
			? ent->d_name
			: nullptr;
	}

	[[gnu::pure]]
	FileDescriptor GetFileDescriptor() const noexcept;
};
