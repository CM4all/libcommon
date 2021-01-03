/*
 * Copyright 2007-2021 CM4all GmbH
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

#ifndef SSL_GENERAL_NAME_HXX
#define SSL_GENERAL_NAME_HXX

#include "util/StringView.hxx"

#include <openssl/x509v3.h>

#include <utility>
#include <algorithm>

#include <assert.h>

namespace OpenSSL {

/**
 * An unmanaged GENERAL_NAME* wrapper.
 */
class GeneralName {
	GENERAL_NAME *value = nullptr;

public:
	GeneralName() = default;
	constexpr GeneralName(GENERAL_NAME *_value):value(_value) {}

	friend void swap(GeneralName &a, GeneralName &b) {
		std::swap(a.value, b.value);
	}

	constexpr operator bool() const {
		return value != nullptr;
	}

	constexpr GENERAL_NAME *get() {
		return value;
	}

	GENERAL_NAME *release() {
		return std::exchange(value, nullptr);
	}

	void clear() {
		assert(value != nullptr);

		GENERAL_NAME_free(release());
	}

	int GetType() const {
		assert(value != nullptr);

		return value->type;
	}

	StringView GetDnsName() const {
		assert(value != nullptr);
		assert(value->type == GEN_DNS);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
		const unsigned char *data = ASN1_STRING_get0_data(value->d.dNSName);
#else
		unsigned char *data = ASN1_STRING_data(value->d.dNSName);
#endif
		if (data == nullptr)
			return nullptr;

		int length = ASN1_STRING_length(value->d.dNSName);
		if (length < 0)
			return nullptr;

		return {(const char*)data, (size_t)length};
	}
};

/**
 * A managed GENERAL_NAME* wrapper.
 */
class UniqueGeneralName : public GeneralName {
public:
	UniqueGeneralName() = default;
	explicit UniqueGeneralName(GENERAL_NAME *_value):GeneralName(_value) {}

	UniqueGeneralName(GeneralName &&src)
		:GeneralName(src.release()) {}

	~UniqueGeneralName() {
		if (*this)
			clear();
	}

	GeneralName &operator=(GeneralName &&src) {
		swap(*this, src);
		return *this;
	}
};

static inline UniqueGeneralName
ToDnsName(const char *value)
{
	return UniqueGeneralName(a2i_GENERAL_NAME(nullptr, nullptr, nullptr,
						  GEN_DNS,
						  const_cast<char *>(value), 0));
}

class GeneralNames {
	GENERAL_NAMES *value = nullptr;

public:
	constexpr GeneralNames(GENERAL_NAMES *_value):value(_value) {}

	friend void swap(GeneralNames &a, GeneralNames &b) {
		std::swap(a.value, b.value);
	}

	constexpr operator bool() const {
		return value != nullptr;
	}

	constexpr GENERAL_NAMES *get() {
		return value;
	}

	GENERAL_NAMES *release() {
		return std::exchange(value, nullptr);
	}

	void clear() {
		assert(value != nullptr);

		GENERAL_NAMES_free(release());
	}

	size_t size() const {
		assert(value != nullptr);

		return sk_GENERAL_NAME_num(value);
	}

	GeneralName operator[](size_t i) const {
		assert(value != nullptr);

		return sk_GENERAL_NAME_value(value, i);
	}

	void push_back(UniqueGeneralName &&n) {
		sk_GENERAL_NAME_push(value, n.release());
	}

	class const_iterator {
		const GeneralNames *sk;
		size_t i;

	public:
		constexpr const_iterator(const GeneralNames &_sk, size_t _i)
			:sk(&_sk), i(_i) {}

		const_iterator &operator++() {
			++i;
			return *this;
		}

		constexpr bool operator==(const_iterator other) const {
			return sk == other.sk && i == other.i;
		}

		constexpr bool operator!=(const_iterator other) const {
			return !(*this == other);
		}

		GeneralName operator*() const {
			return (*sk)[i];
		}

		GeneralName operator->() const {
			return (*sk)[i];
		}
	};

	const_iterator begin() const {
		return const_iterator(*this, 0);
	}

	const_iterator end() const {
		return const_iterator(*this, size());
	}
};

/**
 * A managed GENERAL_NAMES* wrapper.
 */
class UniqueGeneralNames : public GeneralNames {
public:
	UniqueGeneralNames()
		:GeneralNames(sk_GENERAL_NAME_new_null()) {}

	explicit UniqueGeneralNames(GENERAL_NAMES *_value):GeneralNames(_value) {}

	UniqueGeneralNames(GeneralNames &&src)
		:GeneralNames(src.release()) {}

	~UniqueGeneralNames() {
		if (*this)
			clear();
	}

	UniqueGeneralNames &operator=(UniqueGeneralNames &&src) {
		swap(*this, src);
		return *this;
	}
};

}

#endif
