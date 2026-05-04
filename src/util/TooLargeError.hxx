// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

/**
 * This class gets thrown by certain functions when the value passed
 * to it is too large (to fit in the destination buffer).  It is a
 * trivial empty class to reduce the runtime overhead.  Catch blocks
 * should either handle it directly or throw a different exception.
 */
class TooLargeError {};
