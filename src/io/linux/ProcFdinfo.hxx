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
 */
unsigned
ReadPidfdPid(FileDescriptor pidfd);
