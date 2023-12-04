// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Protocol.hxx"
#include "net/SendMessage.hxx"
#include "io/Iovec.hxx"
#include "util/CRC32.hxx"
#include "util/SpanCast.hxx"
#include "util/StaticVector.hxx"

class DatagramBuilder {
	SpawnAccessory::DatagramHeader header;

	StaticVector<struct iovec, 16> v;

public:
	DatagramBuilder() noexcept {
		header.magic = SpawnAccessory::MAGIC;

		AppendRaw(std::span{&header, 1});
	}

	DatagramBuilder(const DatagramBuilder &) = delete;
	DatagramBuilder &operator=(const DatagramBuilder &) = delete;

	template<typename T, std::size_t Extent>
	void AppendRaw(std::span<T, Extent> s) noexcept {
		v.push_back(MakeIovec(s));
	}

	void AppendPadded(std::span<const std::byte> b) noexcept {
		AppendRaw(b);

		const size_t padding_size = (-b.size()) & 3;
		static constexpr std::byte padding[3]{};
		AppendRaw(std::span{padding, padding_size});
	}

	template<typename T>
	void AppendPadded(std::span<const T> s) noexcept {
		AppendPadded(std::as_bytes(s));
	}

	void Append(const SpawnAccessory::RequestHeader &rh) noexcept {
		AppendRaw(ReferenceAsBytes(rh));
	}

	void Append(const SpawnAccessory::ResponseHeader &rh) noexcept {
		AppendRaw(std::span{&rh, 1});
	}

	MessageHeader Finish() noexcept {
		CRC32State crc;

		for (const auto &i : std::span{v}.subspan(1))
			crc.Update(ToSpan(i));

		header.crc = crc.Finish();

		return std::span{v};
	}
};
