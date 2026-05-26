// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Cache.hxx"
#include "SharedRegex.hxx"
#include "UniqueRegex.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/djb_hash.hxx"
#include "util/SpanCast.hxx"

#include <string>

namespace Pcre {

constexpr std::size_t
Cache::Key::Hash::operator()(const Key &key) const noexcept
{
	return djb_hash(AsBytes(key.pattern)) ^ key.options;
}

struct Cache::Item final
	: IntrusiveHashSetHook<IntrusiveHookMode::AUTO_UNLINK>,
	  SharedAnchor
{
	const std::string pattern;
	const int options;

	UniqueRegex regex;

	[[nodiscard]]
	explicit Item(std::string_view _pattern, int _options)
		:pattern(_pattern), options(_options)
	{
		regex.Compile(_pattern, _options);
	}

	SharedRegex Get() noexcept {
		return SharedRegex{regex, *this};
	}

	/* virtual methods from SharedAnchor */
	void OnAbandoned() noexcept override {
		delete this;
	}
};

constexpr Cache::Key
Cache::ItemGetKey::operator()(const Item &item) const noexcept
{
	return {item.pattern, item.options};
}

Cache::Cache() noexcept = default;

Cache::~Cache() noexcept
{
	items.clear_and_dispose(DeleteDisposer{});
}

SharedRegex
Cache::Get(std::string_view pattern, int options)
{
	auto [it, inserted] = items.insert_check(Key{pattern, options});
	if (!inserted)
		return it->Get();

	auto *item = new Item(pattern, options);
	items.insert_commit(it, *item);
	return item->Get();
}

} // namespace Pcre
