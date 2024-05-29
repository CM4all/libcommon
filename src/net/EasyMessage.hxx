// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>
#include <span>

class SocketDescriptor;
class FileDescriptor;
class UniqueFileDescriptor;

/**
 * Sends a message with a contiguous payload and one (optional) file
 * descriptor.
 *
 * Throws on error.
 */
void
EasySendMessage(SocketDescriptor s, std::span<const std::byte> payload,
		FileDescriptor fd);

/**
 * Sends a message with a null byte payload and one (optional) file
 * descriptor.
 *
 * Throws on error.
 */
void
EasySendMessage(SocketDescriptor s, FileDescriptor fd);

/**
 * Receive a message sent by EasySendMessage() (with a null byte
 * payload and returning the contained file descriptor).
 *
 * Returns an "undefined" UniqueFileDescriptor if the message contains
 * no file descriptor.
 *
 * Throws on error.
 */
UniqueFileDescriptor
EasyReceiveMessageWithOneFD(SocketDescriptor s);
