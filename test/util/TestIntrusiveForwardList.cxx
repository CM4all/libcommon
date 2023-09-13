// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/IntrusiveForwardList.hxx"

#include <gtest/gtest.h>

#include <string>

namespace {

struct CharItem final : IntrusiveForwardListHook {
	char ch;

	constexpr CharItem(char _ch) noexcept:ch(_ch) {}
};

static std::string
ToString(const auto &list) noexcept
{
	std::string result;
	for (const auto &i : list)
		result.push_back(i.ch);
	return result;
}

} // anonymous namespace

TEST(IntrusiveForwardList, Basic)
{
	using Item = CharItem;

	Item items[]{'a', 'b', 'c'};

	IntrusiveForwardList<CharItem> list;
	ASSERT_EQ(ToString(list), "");
	list.reverse();
	ASSERT_EQ(ToString(list), "");

	for (auto &i : items)
		list.push_front(i);

	ASSERT_EQ(ToString(list), "cba");

	list.reverse();
	ASSERT_EQ(ToString(list), "abc");

	list.pop_front();
	ASSERT_EQ(ToString(list), "bc");
	list.reverse();
	ASSERT_EQ(ToString(list), "cb");

	/* move constructor */
	auto list2 = std::move(list);
	ASSERT_EQ(ToString(list2), "cb");
	ASSERT_EQ(ToString(list), "");

	/* move operator */
	list = std::move(list2);
	ASSERT_EQ(ToString(list), "cb");
	ASSERT_EQ(ToString(list2), "");
}

TEST(IntrusiveForwardList, ConstantTimeSize)
{
	using Item = CharItem;

	Item items[]{'a', 'b', 'c'};

	IntrusiveForwardList<
		CharItem, IntrusiveForwardListBaseHookTraits<CharItem>,
		IntrusiveForwardListOptions{.constant_time_size = true}> list;
	ASSERT_EQ(ToString(list), "");
	ASSERT_EQ(list.size(), 0U);

	list.reverse();
	ASSERT_EQ(ToString(list), "");
	ASSERT_EQ(list.size(), 0U);

	for (auto &i : items)
		list.push_front(i);

	ASSERT_EQ(ToString(list), "cba");
	ASSERT_EQ(list.size(), 3U);

	list.reverse();
	ASSERT_EQ(ToString(list), "abc");
	ASSERT_EQ(list.size(), 3U);

	list.pop_front();
	ASSERT_EQ(ToString(list), "bc");
	ASSERT_EQ(list.size(), 2U);
	list.reverse();
	ASSERT_EQ(ToString(list), "cb");
	ASSERT_EQ(list.size(), 2U);

	/* move constructor */
	auto list2 = std::move(list);
	ASSERT_EQ(ToString(list2), "cb");
	ASSERT_EQ(list2.size(), 2U);
	ASSERT_EQ(ToString(list), "");
	ASSERT_EQ(list.size(), 0U);

	/* move operator */
	list = std::move(list2);
	ASSERT_EQ(ToString(list), "cb");
	ASSERT_EQ(list.size(), 2U);
	ASSERT_EQ(ToString(list2), "");
	ASSERT_EQ(list2.size(), 0U);
}
