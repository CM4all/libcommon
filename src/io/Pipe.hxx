// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "UniqueFileDescriptor.hxx"

#include <utility>

/**
 * Wrapper for pipe().
 *
 * Throws on error.
 */
std::pair<UniqueFileDescriptor, UniqueFileDescriptor>
CreatePipe();

/**
 * Like CreatePipe(), but with O_NONBLOCK (if available).
 */
std::pair<UniqueFileDescriptor, UniqueFileDescriptor>
CreatePipeNonBlock();
