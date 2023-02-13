// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
