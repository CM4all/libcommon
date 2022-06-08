/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Protocol.hxx"
#include "net/SendMessage.hxx"
#include "io/Iovec.hxx"
#include "util/StaticVector.hxx"

#include <boost/crc.hpp>

class DatagramBuilder {
	SpawnDaemon::DatagramHeader header;

	StaticVector<struct iovec, 16> v;

public:
	DatagramBuilder() noexcept {
		header.magic = SpawnDaemon::MAGIC;

		AppendRaw(std::span{&header, 1});
	}

	DatagramBuilder(const DatagramBuilder &) = delete;
	DatagramBuilder &operator=(const DatagramBuilder &) = delete;

	template<typename T>
	void AppendRaw(std::span<T> s) noexcept {
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

	void Append(const SpawnDaemon::RequestHeader &rh) noexcept {
		AppendRaw(std::as_bytes(std::span{&rh, 1}));
	}

	void Append(const SpawnDaemon::ResponseHeader &rh) noexcept {
		AppendRaw(std::span{&rh, 1});
	}

	MessageHeader Finish() noexcept {
		boost::crc_32_type crc;
		crc.reset();

		for (size_t i = 1; i < v.size(); ++i)
			crc.process_bytes(v[i].iov_base, v[i].iov_len);

		header.crc = crc.checksum();

		return std::span{v};
	}
};
