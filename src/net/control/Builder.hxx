// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Padding.hxx"
#include "Protocol.hxx"
#include "util/ByteOrder.hxx"
#include "util/SpanCast.hxx"

#include <initializer_list>
#include <span>
#include <string>

namespace BengControl {

class Builder {
	std::string data;

public:
	Builder() noexcept {
		static constexpr uint32_t magic = ToBE32(MAGIC);
		AppendT(magic);
	}

	bool empty() const noexcept {
		// this object is empty if it contains only the "magic"
		return size() <= 4;
	}

	void reset() noexcept {
		// erase everything but the 4 "magic" bytes
		data.erase(4);
	}

	void Add(Command cmd) noexcept {
		AppendT(Header{0U, ToBE16(uint16_t(cmd))});
	}

	void Add(Command cmd,
		 std::span<const std::byte> payload) noexcept {
		AppendHeader(cmd, payload.size());
		AppendPadded(payload);
	}

	void Add(Command cmd,
		 std::initializer_list<std::span<const std::byte>> payload) noexcept {
		std::size_t total_size = 0;
		for (const auto i : payload)
			total_size += i.size();

		AppendHeader(cmd, total_size);
		for (const auto i : payload)
			Append(i);

		AppendPadding(total_size);
	}

	void Add(Command cmd,
		 std::string_view payload) noexcept {
		Add(cmd, AsBytes(payload));
	}

	std::size_t size() const noexcept {
		return data.size();
	}

	operator std::span<const std::byte>() const noexcept {
		return AsBytes(data);
	}

private:
	void Append(std::string_view s) noexcept {
		data.append(s);
	}

	void Append(std::span<const std::byte> s) noexcept {
		Append(ToStringView(s));
	}

	void AppendPadded(std::span<const std::byte> s) noexcept {
		Append(s);
		AppendPadding(s.size());
	}

	void AppendT(const auto &s) noexcept {
		Append(ReferenceAsBytes(s));
	}

	void AppendHeader(Command cmd, std::size_t length) noexcept {
		const Header header{
			.length = ToBE16(static_cast<uint16_t>(length)),
			.command = ToBE16(static_cast<uint16_t>(cmd)),
		};

		AppendT(header);
	}

	/**
	 * Append padding for a payload with the specified size.
	 */
	void AppendPadding(std::size_t length) noexcept {
		data.append(PaddingSize(length), '\0');
	}
};

} // namespace BengControl
