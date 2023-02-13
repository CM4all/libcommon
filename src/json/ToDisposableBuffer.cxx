// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ToDisposableBuffer.hxx"
#include "util/AllocatedArray.hxx"
#include "util/DisposableBuffer.hxx"

#include <boost/json/serializer.hpp>

namespace Json {

DisposableBuffer
ToDisposableBuffer(const boost::json::value &v) noexcept
{
	boost::json::serializer s;
	s.reset(&v);

	AllocatedArray<char> array;
	std::size_t size = 0;

	while (!s.done()) {
		char buffer[BOOST_JSON_STACK_BUFFER_SIZE];
		auto r = s.read(buffer);

		array.GrowPreserve(size + r.size(), size);
		std::copy(r.begin(), r.end(), &array[size]);
		size += r.size();
	}

	return {ToDeleteArray(array.release().data()), size};
}

} // namespace Json
