// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class FileDescriptor;

/**
 * Convert the specified pidfd to a regular PID by reading the "Pid:"
 * line in /proc/self/fdinfo/PIDFD.
 *
 * Throws on error.
 *
 * @return the PID (or -1 if the process has already exited and has
 * been reaped)
 */
int
ReadPidfdPid(FileDescriptor pidfd);
