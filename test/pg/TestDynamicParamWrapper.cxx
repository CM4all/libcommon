// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "pg/DynamicParamWrapper.hxx"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

/**
 * Like Pg::Connection::ExecuteDynamic(), allocate exactly Count()
 * slots and let Fill() write them.
 */
template<typename T>
static std::vector<std::string>
Fill(const T &t)
{
	const std::size_t n = Pg::DynamicParamWrapper<T>::Count(t);
	const auto values = std::make_unique_for_overwrite<const char *[]>(n);
	const auto lengths = std::make_unique_for_overwrite<int[]>(n);
	const auto formats = std::make_unique_for_overwrite<int[]>(n);

	const Pg::DynamicParamWrapper<T> w(t);
	const std::size_t filled = w.Fill(values.get(), lengths.get(),
					  formats.get());
	EXPECT_EQ(filled, n);

	return {values.get(), values.get() + filled};
}

TEST(PgDynamicParamWrapper, Scalar)
{
	const std::string value{"foo"};
	EXPECT_EQ(Pg::DynamicParamWrapper<std::string>::Count(value), 1U);
	EXPECT_EQ(Fill(value), std::vector<std::string>{"foo"});
}

TEST(PgDynamicParamWrapper, Vector)
{
	using V = std::vector<std::string>;

	const V value{"a", "b", "c"};
	EXPECT_EQ(Pg::DynamicParamWrapper<V>::Count(value), 3U);
	EXPECT_EQ(Fill(value), value);

	const V empty;
	EXPECT_EQ(Pg::DynamicParamWrapper<V>::Count(empty), 0U);
	EXPECT_TRUE(Fill(empty).empty());
}

TEST(PgDynamicParamWrapper, NestedVector)
{
	using V = std::vector<std::vector<std::string>>;

	const V value{
		{},
		{"a"},
		{"b", "c", "d"},
	};

	EXPECT_EQ(Pg::DynamicParamWrapper<V>::Count(value), 4U);
	EXPECT_EQ(Fill(value), (std::vector<std::string>{"a", "b", "c", "d"}));
}
