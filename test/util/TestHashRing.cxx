// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "util/HashRing.hxx"

#include <gtest/gtest.h>

TEST(HashRingTest, NoReplicas)
{
	struct Node {
		unsigned hash;
	};

	struct NodeHasher {
		unsigned operator()(const Node &node, size_t) const {
			return node.hash;
		}
	};

	HashRing<const Node, unsigned, 16, 1> hr;

	static constexpr std::array<Node, 3> nodes{{{2}, {42}, {4711}}};
	hr.Build(nodes, NodeHasher());

	ASSERT_EQ(&hr.Pick(0), &nodes[1]);
	ASSERT_EQ(&hr.Pick(1), &nodes[1]);
	ASSERT_EQ(&hr.Pick(2), &nodes[0]);
	ASSERT_EQ(&hr.Pick(3), &nodes[0]);
	ASSERT_EQ(&hr.Pick(4), &nodes[0]);
	ASSERT_EQ(&hr.Pick(5), &nodes[0]);
	ASSERT_EQ(&hr.Pick(6), &nodes[0]);
	ASSERT_EQ(&hr.Pick(7), &nodes[2]);
	ASSERT_EQ(&hr.Pick(8), &nodes[2]);
	ASSERT_EQ(&hr.Pick(9), &nodes[2]);
	ASSERT_EQ(&hr.Pick(10), &nodes[1]);
	ASSERT_EQ(&hr.Pick(11), &nodes[1]);
	ASSERT_EQ(&hr.Pick(12), &nodes[1]);
	ASSERT_EQ(&hr.Pick(13), &nodes[1]);
	ASSERT_EQ(&hr.Pick(14), &nodes[1]);
	ASSERT_EQ(&hr.Pick(15), &nodes[1]);
}

TEST(HashRingTest, OneReplica)
{
	struct Node {
		unsigned hash;
	};

	struct NodeHasher {
		unsigned operator()(const Node &node, size_t replica) const {
			return node.hash + replica * 7;
		}
	};

	HashRing<const Node, unsigned, 16, 2> hr;

	static constexpr std::array<Node, 3> nodes{{{2}, {42}, {4711}}};
	hr.Build(nodes, NodeHasher());

	ASSERT_EQ(&hr.Pick(0), &nodes[2]);
	ASSERT_EQ(&hr.Pick(1), &nodes[1]);
	ASSERT_EQ(&hr.Pick(2), &nodes[0]);
	ASSERT_EQ(&hr.Pick(3), &nodes[0]);
	ASSERT_EQ(&hr.Pick(4), &nodes[0]);
	ASSERT_EQ(&hr.Pick(5), &nodes[0]);
	ASSERT_EQ(&hr.Pick(6), &nodes[0]);
	ASSERT_EQ(&hr.Pick(7), &nodes[2]);
	ASSERT_EQ(&hr.Pick(8), &nodes[2]);
	ASSERT_EQ(&hr.Pick(9), &nodes[0]);
	ASSERT_EQ(&hr.Pick(10), &nodes[1]);
	ASSERT_EQ(&hr.Pick(11), &nodes[1]);
	ASSERT_EQ(&hr.Pick(12), &nodes[1]);
	ASSERT_EQ(&hr.Pick(13), &nodes[1]);
	ASSERT_EQ(&hr.Pick(14), &nodes[2]);
	ASSERT_EQ(&hr.Pick(15), &nodes[2]);

	ASSERT_EQ(&hr.FindNext(0).second, &nodes[1]);
	ASSERT_EQ(&hr.FindNext(1).second, &nodes[0]);
	ASSERT_EQ(&hr.FindNext(2).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(3).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(4).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(5).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(6).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(7).second, &nodes[0]);
	ASSERT_EQ(&hr.FindNext(8).second, &nodes[0]);
	ASSERT_EQ(&hr.FindNext(9).second, &nodes[1]);
	ASSERT_EQ(&hr.FindNext(10).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(11).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(12).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(13).second, &nodes[2]);
	ASSERT_EQ(&hr.FindNext(14).second, &nodes[1]);
	ASSERT_EQ(&hr.FindNext(15).second, &nodes[1]);
}
