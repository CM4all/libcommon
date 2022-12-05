/*
 * Copyright 2004-2022 CM4all GmbH
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

#include "RecursiveCopy.hxx"
#include "UniqueFileDescriptor.hxx"
#include "MakeDirectory.hxx"
#include "DirectoryReader.hxx"
#include "Open.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"

#include <array>
#include <cstddef>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

struct RecursiveCopyContext {
	/**
	 * @see RECURSIVE_COPY_NO_OVERWRITE
	 */
	const bool overwrite;

	constexpr RecursiveCopyContext(unsigned options) noexcept
		:overwrite(!(options & RECURSIVE_COPY_NO_OVERWRITE)) {}
};

static void
RecursiveCopy(RecursiveCopyContext &ctx,
	      FileDescriptor src_parent, const char *src_filename,
	      FileDescriptor dst_parent, const char *dst_filename);

/**
 * Attempt to use copy_file_range() to copy all data from one file to
 * the other.
 *
 * Throws on error.
 *
 * @return true on success (all data has been copied), false if
 * copy_file_range() is not supported (no data has been copied)
 */
static bool
CopyFileRange(FileDescriptor src, FileDescriptor dst, off_t size)
{
	/* check if copy_file_range() works */
	auto nbytes = copy_file_range(src.Get(), nullptr, dst.Get(), nullptr,
				      size, 0);
	if (nbytes <= 0)
		/* nah */
		return false;

	/* hooray, copy_file_range() works */
	size -= nbytes;

	while (size > 0) {
		nbytes = copy_file_range(src.Get(), nullptr,
					 dst.Get(), nullptr,
					 size, 0);
		if (nbytes <= 0) [[unlikely]] {
			if (nbytes == 0)
				throw std::runtime_error{"Unexpected end of file"};

			throw MakeErrno("Failed to copy file data");
		}

		size -= nbytes;
	}

	return true;
}

/**
 * Copy all data from one file to the other.
 *
 * Throws on error.
 */
static void
CopyRegularFile(FileDescriptor src, FileDescriptor dst, off_t size)
{
	if (size <= 0)
		return;

	if (CopyFileRange(src, dst, size))
		return;

	posix_fadvise(src.Get(), 0, size, POSIX_FADV_SEQUENTIAL);

	fallocate(dst.Get(), FALLOC_FL_KEEP_SIZE, 0, size);

	while (size > 0) {
		std::array<std::byte, 65536> buffer;
		const auto nbytes1 =
			src.Read(buffer.data(), buffer.size());
		if (nbytes1 <= 0) [[unlikely]] {
			if (nbytes1 == 0)
				throw std::runtime_error{"Unexpected end of file"};

			throw MakeErrno("Failed to read file");
		}

		ssize_t nbytes2 = dst.Write(buffer.data(), (size_t)nbytes1);
		if (nbytes2 < nbytes1) [[unlikely]] {
			if (nbytes2 < 0)
				throw MakeErrno("Failed to write file");

			throw std::runtime_error{"Short write"};
		}

		size -= nbytes2;
	}
}

/**
 * Create a regular file.  If one already exists, it is deleted.
 */
static UniqueFileDescriptor
CreateRegularFile(FileDescriptor parent, const char *filename, bool overwrite)
{
	UniqueFileDescriptor dst;

	/* optimistic create with O_EXCL */
	if (dst.Open(parent, filename, O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW))
		return dst;

	if (const int e = errno; e != EEXIST)
		throw FormatErrno(e, "Failed to create '%s'", filename);

	if (!overwrite)
		return {};

	/* already exists: delete it (so we create a new inode for the
	   new file) */
	if (unlinkat(parent.Get(), filename, 0) < 0)
		if (const int e = errno; e != ENOENT)
			throw FormatErrno(e, "Failed to delete '%s'", filename);

	/* ... and try again */
	if (!dst.Open(parent, filename, O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW))
		throw FormatErrno("Failed to create '%s'", filename);

	return dst;
}

static void
CopyRegularFile(FileDescriptor src,
		FileDescriptor dst_parent, const char *dst_filename,
		off_t size,
		bool overwrite)
{
	const auto dst = CreateRegularFile(dst_parent, dst_filename,
					   overwrite);
	if (!dst.IsDefined())
		return;

	CopyRegularFile(src, dst, size);
}

[[gnu::pure]]
static bool
IsSpecialFilename(const char *s) noexcept
{
	return s[0] == '.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0));
}

/**
 * Copy the contents of the given source directory to the given
 * destination directory.
 */
static void
RecursiveCopyDirectory(RecursiveCopyContext &ctx,
		       DirectoryReader &&src, FileDescriptor dst)
{
	while (auto *name = src.Read())
		if (!IsSpecialFilename(name))
			RecursiveCopy(ctx,
				      src.GetFileDescriptor(), name,
				      dst, name);
}

/**
 * Copy the contents of the given source directory to a newly created
 * directory.
 */
static void
RecursiveCopyDirectory(RecursiveCopyContext &ctx,
		       DirectoryReader &&src,
		       FileDescriptor dst_parent, const char *dst_filename)
{
	if (*dst_filename == 0)
		RecursiveCopyDirectory(ctx, std::move(src), dst_parent);
	else
		RecursiveCopyDirectory(ctx, std::move(src),
				       MakeDirectory(dst_parent, dst_filename));
}

static void
RecursiveCopy(RecursiveCopyContext &ctx,
	      UniqueFileDescriptor &&src, const struct stat &st,
	      FileDescriptor dst_parent, const char *dst_filename)
{
	if (S_ISDIR(st.st_mode))
		RecursiveCopyDirectory(ctx,
				       DirectoryReader{std::move(src)},
				       dst_parent, dst_filename);
	else if (S_ISREG(st.st_mode))
		CopyRegularFile(src, dst_parent, dst_filename,
				st.st_size,
				ctx.overwrite);
	else {
		// TODO
	}
}

static void
CreateSymlink(FileDescriptor parent, const char *filename, const char *target,
	      bool overwrite)
{
	if (symlinkat(target, parent.Get(), filename) == 0)
		return;

	if (const int e = errno; e != EEXIST)
		throw FormatErrno(e, "Failed to create '%s'",
				  filename);

	if (!overwrite)
		return;

	if (unlinkat(parent.Get(), filename, 0) < 0)
		if (const int e = errno; e != ENOENT)
			throw FormatErrno(e, "Failed to delete '%s'", filename);

	if (symlinkat(target, parent.Get(), filename) < 0)
		throw FormatErrno("Failed to create '%s'",
				  filename);
}

static void
CopySymlink(FileDescriptor src_parent, const char *src_filename,
	    FileDescriptor dst_parent, const char *dst_filename,
	    bool overwrite)
{
	char buffer[4096];

	ssize_t length  = readlinkat(src_parent.Get(), src_filename,
				     buffer, sizeof(buffer));
	if (length < 0)
		throw FormatErrno("Failed to read symlink '%s'", src_filename);

	if ((std::size_t)length == sizeof(buffer))
		throw FormatRuntimeError("Symlink '%s' is too long",
					 src_filename);

	buffer[length] = 0;

	CreateSymlink(dst_parent, dst_filename, buffer, overwrite);
}

static void
RecursiveCopy(RecursiveCopyContext &ctx,
	      FileDescriptor src_parent, const char *src_filename,
	      FileDescriptor dst_parent, const char *dst_filename)
{
	UniqueFileDescriptor src;

	/* optimistic open() - this works for regular files and
	   directories */
	if (!src.Open(src_parent, src_filename, O_RDONLY|O_NOFOLLOW)) {
		switch (int e = errno) {
		case ELOOP:
			/* due to O_NOFOLLOW, symlinks fail with
			   ELOOP, so copy the symlink */
			CopySymlink(src_parent, src_filename,
				    dst_parent, dst_filename,
				    ctx.overwrite);
			return;

		default:
			throw FormatErrno(e, "Failed to open '%s'",
					  src_filename);
		}
	}

	struct stat st;
	if (fstat(src.Get(), &st) < 0)
		throw FormatErrno("Failed to stat '%s'", src_filename);

	RecursiveCopy(ctx, std::move(src), st, dst_parent, dst_filename);
}

void
RecursiveCopy(FileDescriptor src_parent, const char *src_filename,
	      FileDescriptor dst_parent, const char *dst_filename,
	      unsigned options)
{
	RecursiveCopyContext ctx{options};
	RecursiveCopy(ctx,
		      src_parent, src_filename,
		      dst_parent, dst_filename);
}
