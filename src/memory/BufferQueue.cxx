// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "BufferQueue.hxx"

#include <algorithm>

BufferQueue::BufferQueue() noexcept = default;
BufferQueue::~BufferQueue() noexcept = default;

void
BufferQueue::Push(std::span<const std::byte> src) noexcept
{
	/* try to append to the last existing buffer (if there is
	   any) */
	if (!buffers.empty()) {
		auto &b = buffers.back();
		assert(b.IsDefined());

		const std::size_t nbytes = b.MoveFrom(src);
		src = src.subspan(nbytes);
	}

	/* create more buffers for remaining data */
	while (!src.empty()) {
		buffers.emplace_back();
		auto &b = buffers.back();
		b.Allocate();

		const std::size_t nbytes = b.MoveFrom(src);
		src = src.subspan(nbytes);
	}
}

std::size_t
BufferQueue::GetAvailable() const noexcept
{
	std::size_t result = 0;
	for (const auto &i : buffers)
		result += i.GetAvailable();
	return result;
}

std::span<const std::byte>
BufferQueue::Read() const noexcept
{
	if (buffers.empty())
		return {};

	return buffers.front().Read();
}

void
BufferQueue::Consume(std::size_t nbytes) noexcept
{
	if (nbytes == 0)
		return;

	assert(!buffers.empty());

	auto &b = buffers.front();
	assert(b.IsDefined());
	assert(b.GetAvailable() >= nbytes);
	b.Consume(nbytes);

	if (b.empty())
		buffers.pop_front();
}

std::size_t
BufferQueue::Skip(std::size_t nbytes) noexcept
{
	std::size_t result = 0;
	while (!empty()) {
		auto &b = buffers.front();
		const std::size_t available = b.GetAvailable();
		std::size_t consume = std::min(nbytes, available);
		result += consume;
		nbytes -= consume;
		if (consume < available) {
			b.Consume(consume);
			break;
		}

		buffers.pop_front();
	}

	return result;
}
