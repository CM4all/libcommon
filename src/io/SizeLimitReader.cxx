// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SizeLimitReader.hxx"

#include <cassert>
#include <stdexcept>

std::size_t
SizeLimitReader::Read(std::span<std::byte> dest)
{
	assert(!dest.empty());

	if (dest.size() > remaining + 1)
		/* read one more than the limit to see if we're really
		   going above the limit */
		dest = dest.first(remaining + 1);

	const auto nbytes = next.Read(dest);
	if (nbytes > remaining)
		throw std::runtime_error{"File too large"};

	remaining -= nbytes;
	return nbytes;
}
