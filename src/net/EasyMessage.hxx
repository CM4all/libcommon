// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstdint>
#include <exception>
#include <span>
#include <string_view>

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
 * Sends a message with the specified error message.  The function
 * EasyReceiveMessageWithOneFD() will decode and rethrow it as a
 * std::runtime_error.
 *
 * Throws on error.
 */
void
EasySendError(SocketDescriptor s, std::string_view text);

/**
 * Sends a message with information about the specified exception.
 * The function EasyReceiveMessageWithOneFD() will decode and rethrow
 * it.
 *
 * Throws on error.
 */
void
EasySendError(SocketDescriptor s, std::exception_ptr error);

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
