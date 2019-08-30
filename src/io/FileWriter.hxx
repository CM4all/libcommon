/*
 * Copyright (C) 2011-2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef FILE_WRITER_HXX
#define FILE_WRITER_HXX

#include "UniqueFileDescriptor.hxx"

#include <string>

/**
 * Create a new file or replace an existing file.  This class attempts
 * to make this atomic: if an error occurs, it attempts to restore the
 * previous state.
 */
class FileWriter {
	const std::string path;

	std::string tmp_path;

	UniqueFileDescriptor fd;

public:
	/**
	 * Throws std::system_error on error.
	 */
	explicit FileWriter(const char *_path);

	~FileWriter() noexcept {
		if (fd.IsDefined())
			Cancel();
	}

	FileDescriptor GetFileDescriptor() noexcept {
		return fd;
	}

	/**
	 * Attempt to allocate space on the file system.  This may
	 * speed up following writes, and may reduce file system
	 * fragmentation.  This is a hint, and there is no error
	 * checking.
	 */
	void Allocate(off_t size) noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Write(const void *data, size_t size);

	/**
	 * Throws std::system_error on error.
	 */
	void Commit();

	/**
	 * Attempt to undo the changes by this class.
	 */
	void Cancel() noexcept;
};

#endif
