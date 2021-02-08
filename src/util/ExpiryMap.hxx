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

#pragma once

#include "Expiry.hxx"

#include <map>

/**
 * A map of #Expiry values.  Expired items are deleted on access.
 */
template<typename K>
class ExpiryMap {
	std::map<K, Expiry> map;

public:
	bool empty() const noexcept {
		return map.empty();
	}

	void clear() noexcept {
		map.clear();
	}

	template<typename KK>
	[[gnu::pure]]
	bool IsExpired(KK &&key, Expiry now) noexcept {
		auto i = map.find(std::forward<KK>(key));
		if (i == map.end())
			return true;

		if (i->second.IsExpired(now)) {
			map.erase(i);
			return true;
		}

		return false;
	}

	template<typename KK>
	void Set(KK &&key, Expiry value) noexcept {
		auto i = map.emplace(std::forward<KK>(key), value);
		if (!i.second && i.first->second < value)
			i.first->second = value;
	}

	/**
	 * @return the earliest remaining expiry (which is not yet
	 * expired, of course) or Expiry{} if the map is now empty
	 */
	template<typename F>
	Expiry ForEach(Expiry now, F &&f) noexcept {
		Expiry earliest{};

		for (auto i = map.begin(), end = map.end(); i != end;) {
			if (i->second.IsExpired(now)) {
				i = map.erase(i);
			} else {
				if (earliest == Expiry{} ||
				    i->second < earliest)
					earliest = i->second;

				f(i->first);
				++i;
			}
		}

		return earliest;
	}
};
