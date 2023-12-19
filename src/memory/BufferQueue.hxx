// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "DefaultFifoBuffer.hxx"

#include <iterator>
#include <list>
#include <span>

/**
 * A queue of data stored in #DefaultFifoBuffer.  You can push new
 * data to the tail and consume data from the beginning.
 */
class BufferQueue {
	using BufferList = std::list<DefaultFifoBuffer>;
	BufferList buffers;

public:
	BufferQueue() noexcept;
	~BufferQueue() noexcept;

	BufferQueue(BufferQueue &&) noexcept = default;
	BufferQueue &operator=(BufferQueue &&) noexcept = default;

	[[gnu::pure]]
	bool empty() const noexcept {
		return buffers.empty();
	}

	void Push(std::span<const std::byte> src) noexcept;

	[[gnu::pure]]
	std::size_t GetAvailable() const noexcept;

	std::span<const std::byte> Read() const noexcept;
	void Consume(std::size_t nbytes) noexcept;

	/**
	 * Like Consume(), but can run across several list and the
	 * parameter is allowed to be larger than GetAvailable().
	 */
	std::size_t Skip(std::size_t nbytes) noexcept;

	class const_iterator {
		friend class BufferQueue;
		using Traits = std::iterator_traits<BufferList::const_iterator>;

		BufferList::const_iterator i;

		const_iterator(BufferList::const_iterator _i) noexcept
			:i(_i) {}

	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = typename Traits::difference_type;
		using value_type = std::span<const std::byte>;

		value_type operator*() const noexcept {
			return i->Read();
		}

		auto &operator++() noexcept {
			++i;
			return *this;
		}

		const_iterator operator++(int) noexcept {
			return i++;
		}

		bool operator==(const const_iterator &other) const noexcept {
			return i == other.i;
		}

		bool operator!=(const const_iterator &other) const noexcept {
			return !(*this == other);
		}
	};

	const_iterator begin() const noexcept {
		return {buffers.begin()};
	}

	const_iterator end() const noexcept {
		return {buffers.end()};
	}
};
