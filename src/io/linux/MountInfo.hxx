/*
 * Copyright 2014-2022 CM4all GmbH
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

#include <string>

struct MountInfo {
	/**
	 * The relative path inside the file system which was mounted on
	 * the given mount point.  This is relevant for bind mounts.
	 */
	std::string root;

	/**
	 * The filesystem type.
	 */
	std::string filesystem;

	/**
	 * The device which was mounted on the given mount point.
	 */
	std::string source;

	bool IsDefined() const {
		return !root.empty() || !source.empty();
	}
};

/**
 * Determine which file system is mounted at the given mount point
 * path (exact match required).
 *
 * Throws std::runtime_error on error.
 *
 * @param pid a process id or 0 to obtain information about the
 * current process
 */
MountInfo
ReadProcessMount(unsigned pid, const char *mountpoint);

/**
 * Find a mounted device.
 *
 * Throws std::runtime_error on error.
 *
 * @param pid a process id or 0 to obtain information about the
 * current process
 *
 * @param major_minor a device specification using major and minor
 * number in the form "MAJOR:MINOR"
 */
MountInfo
FindMountInfoByDevice(unsigned pid, const char *major_minor);
