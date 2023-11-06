// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/IntrusiveHashArrayTrie.hxx"

#include <gtest/gtest.h>

#include <string>

namespace {

struct IntItem final : IntrusiveHashArrayTrieHook<IntrusiveHookMode::TRACK> {
	int value;

	IntItem(int _value) noexcept:value(_value) {}

	struct Hash {
		constexpr std::size_t operator()(const IntItem &i) const noexcept {
			return i.value;
		}

		constexpr std::size_t operator()(int i) const noexcept {
			return i;
		}
	};

	struct Equal {
		constexpr bool operator()(const IntItem &a,
					  const IntItem &b) const noexcept {
			return a.value == b.value;
		}
	};
};

} // anonymous namespace

TEST(IntrusiveHashArrayTrie, Basic)
{
	IntItem a{1}, b{2}, c{3}, d{4}, e{5}, f{1};

	IntrusiveHashArrayTrie<IntItem,
			       IntrusiveHashArrayTrieOperators<IntItem::Hash,
							       IntItem::Equal>> set;

	EXPECT_TRUE(set.empty());
	EXPECT_EQ(set.size(), 0);
	EXPECT_EQ(set.find(a), set.end());
	EXPECT_EQ(set.find(b), set.end());
	EXPECT_EQ(set.find(c), set.end());
	EXPECT_EQ(set.find(d), set.end());
	EXPECT_EQ(set.find(e), set.end());
	EXPECT_EQ(set.find(f), set.end());
	EXPECT_FALSE(a.is_linked());
	EXPECT_FALSE(b.is_linked());
	EXPECT_FALSE(c.is_linked());
	EXPECT_FALSE(d.is_linked());
	EXPECT_FALSE(e.is_linked());
	EXPECT_FALSE(f.is_linked());

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunreachable-code-loop-increment"
#endif

	for ([[maybe_unused]] const auto &i : set)
		FAIL();

	auto er = set.equal_range(f);
	EXPECT_EQ(er.first, set.end());
	EXPECT_EQ(er.second, set.end());

	set.insert(a);
	EXPECT_EQ(set.size(), 1);
	EXPECT_NE(set.find(a), set.end());
	EXPECT_EQ(&*set.find(a), &a);
	EXPECT_EQ(set.find(b), set.end());
	EXPECT_EQ(set.find(c), set.end());
	EXPECT_EQ(set.find(d), set.end());
	EXPECT_EQ(set.find(e), set.end());
	EXPECT_EQ(&*set.find(f), &a);
	EXPECT_TRUE(a.is_linked());
	EXPECT_FALSE(b.is_linked());
	EXPECT_FALSE(c.is_linked());
	EXPECT_FALSE(d.is_linked());
	EXPECT_FALSE(e.is_linked());
	EXPECT_FALSE(f.is_linked());

	er = set.equal_range(f);
	EXPECT_NE(er.first, set.end());
	EXPECT_EQ(er.second, set.end());
	EXPECT_EQ(&*er.first, &a);
	EXPECT_EQ(std::next(er.first), er.second);

	set.insert(b);
	EXPECT_EQ(set.size(), 2);
	set.insert(c);
	EXPECT_EQ(set.size(), 3);
	set.insert(d);
	EXPECT_EQ(set.size(), 4);
	set.insert(e);
	EXPECT_EQ(set.size(), 5);
	set.insert(f);
	EXPECT_EQ(set.size(), 6);

	EXPECT_TRUE(a.is_linked());
	EXPECT_TRUE(b.is_linked());
	EXPECT_TRUE(c.is_linked());
	EXPECT_TRUE(d.is_linked());
	EXPECT_TRUE(e.is_linked());
	EXPECT_TRUE(f.is_linked());

	EXPECT_NE(set.find(a), set.end());
	EXPECT_TRUE(&*set.find(a) == &a ||
		    &*set.find(a) == &f);
	EXPECT_NE(set.find(b), set.end());
	EXPECT_EQ(&*set.find(b), &b);
	EXPECT_NE(set.find(c), set.end());
	EXPECT_EQ(&*set.find(c), &c);
	EXPECT_NE(set.find(d), set.end());
	EXPECT_EQ(&*set.find(d), &d);
	EXPECT_NE(set.find(e), set.end());
	EXPECT_EQ(&*set.find(e), &e);
	EXPECT_NE(set.find(f), set.end());
	EXPECT_TRUE(&*set.find(f) == &a ||
		    &*set.find(f) == &f);

	er = set.equal_range(f);
	EXPECT_NE(er.first, set.end());
	EXPECT_EQ(er.second, set.end());
	EXPECT_EQ(&*er.first, &a);
	EXPECT_EQ(&*std::next(er.first), &f);
	EXPECT_EQ(std::next(er.first, 2), er.second);

	EXPECT_EQ(set.find_if(a, [&](auto &i){ return &i == &a; }), set.iterator_to(a));
	EXPECT_EQ(set.find_if(a, [&](auto &i){ return &i == &f; }), set.iterator_to(f));

	EXPECT_TRUE(a.is_linked());
	a.unlink();

	er = set.equal_range(f);
	EXPECT_NE(er.first, set.end());
	EXPECT_EQ(er.second, set.end());
	EXPECT_EQ(&*er.first, &f);
	EXPECT_EQ(std::next(er.first), er.second);

	EXPECT_FALSE(a.is_linked());
	EXPECT_TRUE(b.is_linked());
	EXPECT_TRUE(c.is_linked());
	EXPECT_TRUE(d.is_linked());
	EXPECT_TRUE(e.is_linked());
	EXPECT_TRUE(f.is_linked());

	EXPECT_NE(set.find(a), set.end());
	EXPECT_EQ(&*set.find(a), &f);
	EXPECT_NE(set.find(b), set.end());
	EXPECT_EQ(&*set.find(b), &b);
	EXPECT_NE(set.find(c), set.end());
	EXPECT_EQ(&*set.find(c), &c);
	EXPECT_NE(set.find(d), set.end());
	EXPECT_EQ(&*set.find(d), &d);
	EXPECT_NE(set.find(e), set.end());
	EXPECT_EQ(&*set.find(e), &e);
	EXPECT_NE(set.find(f), set.end());
	EXPECT_EQ(&*set.find(f), &f);

	EXPECT_EQ(set.find_if(a, [&](auto &i){ return &i == &a; }), set.end());
	EXPECT_EQ(set.find_if(a, [&](auto &i){ return &i == &f; }), set.iterator_to(f));

	set.erase(set.iterator_to(b));

	EXPECT_FALSE(a.is_linked());
	EXPECT_FALSE(b.is_linked());
	EXPECT_TRUE(c.is_linked());
	EXPECT_TRUE(d.is_linked());
	EXPECT_TRUE(e.is_linked());
	EXPECT_TRUE(f.is_linked());

	std::vector<int> v;
	for (const auto &i : set)
		v.emplace_back(i.value);

	static constexpr std::array expected_v{3, 4, 5, 1};
	EXPECT_TRUE(std::is_permutation(v.begin(), v.end(),
					expected_v.begin(), expected_v.end()));

	auto other_set = std::move(set);
	EXPECT_EQ(set.size(), 0);
	EXPECT_EQ(other_set.size(), 4);

	other_set.swap(set);
	EXPECT_EQ(set.size(), 4);
	EXPECT_EQ(other_set.size(), 0);

	set.insert(a);
	set.insert(b);

	er = set.equal_range(a);
	EXPECT_NE(er.first, set.end());
	EXPECT_EQ(er.second, set.end());
	EXPECT_EQ(&*er.first, &f);
	EXPECT_EQ(&*std::next(er.first), &a);
	EXPECT_EQ(std::next(er.first, 2), er.second);

	bool found_a = false, found_f = false, found_other = false;
	set.for_each(f, [&](const auto &i){
		if (&i == &a)
			found_a = true;
		else if (&i == &f)
			found_f = true;
		else
			found_other = true;
	});

	EXPECT_TRUE(found_a);
	EXPECT_TRUE(found_f);
	EXPECT_FALSE(found_other);

	EXPECT_EQ(set.remove_and_dispose_key(f, [](auto*){}), 2);
	EXPECT_EQ(set.remove_and_dispose_key(e, [](auto*){}), 1);
	EXPECT_EQ(set.remove_and_dispose_key(e, [](auto*){}), 0);

	er = set.equal_range(f);
	EXPECT_EQ(er.first, set.end());
	EXPECT_EQ(er.second, set.end());

	set.clear_and_dispose([](const auto *){});
	EXPECT_TRUE(set.empty());
}
