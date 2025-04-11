// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "RecursiveCopy.hxx"
#include "CopyRegularFile.hxx"
#include "FileName.hxx"
#include "UniqueFileDescriptor.hxx"
#include "MakeDirectory.hxx"
#include "DirectoryReader.hxx"
#include "Open.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"

#include <array>
#include <cstddef>
#include <cstdint>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/stat.h> // for STX_* (on Musl)

static constexpr int
RecursiveCopyOptionsToStatxMask(unsigned options) noexcept
{
	int mask = STATX_TYPE|STATX_SIZE;
	if (options & RECURSIVE_COPY_ONE_FILESYSTEM)
		mask |= STATX_MNT_ID;
	if (options & RECURSIVE_COPY_PRESERVE_MODE)
		mask |= STATX_MODE;
	if (options & RECURSIVE_COPY_PRESERVE_TIME)
		mask |= STATX_MTIME;
	return mask;
}

struct RecursiveCopyContext {
	uint_least64_t mnt_id{};

	const int statx_mask;

	/**
	 * @see RECURSIVE_COPY_NO_OVERWRITE
	 */
	const bool overwrite;

	const bool one_filesystem;

	const bool preserve_mode, preserve_time;

	constexpr RecursiveCopyContext(unsigned options) noexcept
		:statx_mask(RecursiveCopyOptionsToStatxMask(options)),
		 overwrite(!(options & RECURSIVE_COPY_NO_OVERWRITE)),
		 one_filesystem(options & RECURSIVE_COPY_ONE_FILESYSTEM),
		 preserve_mode(options & RECURSIVE_COPY_PRESERVE_MODE),
		 preserve_time(options & RECURSIVE_COPY_PRESERVE_TIME)
	{
	}
};

static void
Preserve(const RecursiveCopyContext &ctx, const struct statx &stx,
	 FileDescriptor dst, const char *dst_filename)
{
	if (ctx.preserve_mode &&
	    (S_ISDIR(stx.stx_mode)
	     ? fchmodat(dst.Get(), ".", stx.stx_mode & ~S_IFMT,
			0)
	     : fchmod(dst.Get(), stx.stx_mode & ~S_IFMT)) < 0)
		throw FmtErrno("Failed to set mode of {:?}", dst_filename);

	if (ctx.preserve_time) {
		struct timespec times[2];
		times[0].tv_nsec = UTIME_OMIT;
		times[1].tv_sec = stx.stx_mtime.tv_sec;
		times[1].tv_nsec = stx.stx_mtime.tv_nsec;

		if ((S_ISDIR(stx.stx_mode)
		     ? utimensat(dst.Get(), ".", times,
				 AT_SYMLINK_NOFOLLOW)
		     : futimens(dst.Get(), times)) < 0)
			throw FmtErrno("Failed to set time of {:?}",
				       dst_filename);
	}
}

static void
RecursiveCopy(RecursiveCopyContext &ctx,
	      FileDescriptor src_parent, const char *src_filename,
	      FileDescriptor dst_parent, const char *dst_filename);

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
		throw FmtErrno(e, "Failed to create {:?}", filename);

	if (!overwrite)
		return {};

	/* already exists: delete it (so we create a new inode for the
	   new file) */
	if (unlinkat(parent.Get(), filename, 0) < 0)
		if (const int e = errno; e != ENOENT)
			throw FmtErrno(e, "Failed to delete {:?}", filename);

	/* ... and try again */
	if (!dst.Open(parent, filename, O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW))
		throw FmtErrno("Failed to create {:?}", filename);

	return dst;
}

static UniqueFileDescriptor
CopyRegularFile(FileDescriptor src,
		FileDescriptor dst_parent, const char *dst_filename,
		off_t size,
		bool overwrite)
{
	auto dst = CreateRegularFile(dst_parent, dst_filename,
				     overwrite);
	if (dst.IsDefined())
		CopyRegularFile(src, dst, size);

	return dst;
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
		       DirectoryReader &&src, const struct statx &stx,
		       FileDescriptor dst_parent, const char *dst_filename)
{
	if (*dst_filename == 0) {
		RecursiveCopyDirectory(ctx, std::move(src), dst_parent);
		Preserve(ctx, stx, dst_parent, "?");
	} else {
		auto dst = MakeDirectory(dst_parent, dst_filename);
		RecursiveCopyDirectory(ctx, std::move(src), dst);
		Preserve(ctx, stx, dst, dst_filename);
	}
}

static void
RecursiveCopy(RecursiveCopyContext &ctx,
	      UniqueFileDescriptor &&src, const struct statx &stx,
	      FileDescriptor dst_parent, const char *dst_filename)
{
	if (S_ISDIR(stx.stx_mode))
		RecursiveCopyDirectory(ctx,
				       DirectoryReader{std::move(src)},
				       stx,
				       dst_parent, dst_filename);
	else if (S_ISREG(stx.stx_mode)) {
		auto dst = CopyRegularFile(src, dst_parent, dst_filename,
					   stx.stx_size,
					   ctx.overwrite);
		if (dst.IsDefined())
			Preserve(ctx, stx, dst, dst_filename);
	} else {
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
		throw FmtErrno(e, "Failed to create {:?}",
			       filename);

	if (!overwrite)
		return;

	if (unlinkat(parent.Get(), filename, 0) < 0)
		if (const int e = errno; e != ENOENT)
			throw FmtErrno(e, "Failed to delete {:?}", filename);

	if (symlinkat(target, parent.Get(), filename) < 0)
		throw FmtErrno("Failed to create {:?}", filename);
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
		throw FmtErrno("Failed to read symlink {:?}", src_filename);

	if ((std::size_t)length == sizeof(buffer))
		throw FmtRuntimeError("Symlink {:?} is too long",
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
			throw FmtErrno(e, "Failed to open {:?}",
				       src_filename);
		}
	}

	struct statx stx;
	if (statx(src.Get(), "",
		  AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW|AT_STATX_SYNC_AS_STAT,
		  ctx.statx_mask, &stx) < 0)
		throw FmtErrno("Failed to stat {:?}", src_filename);

	if (ctx.one_filesystem) {
		if (ctx.mnt_id == 0)
			/* this is the top-level call - initialize the
			   "device" field */
			ctx.mnt_id = stx.stx_mnt_id;
		else if (stx.stx_mnt_id != ctx.mnt_id)
			/* this is on a different device (filesystem);
			   ignore it */
			return;
	}

	RecursiveCopy(ctx, std::move(src), stx, dst_parent, dst_filename);
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
