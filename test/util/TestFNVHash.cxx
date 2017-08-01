#include "util/FNVHash.hxx"

#include <gtest/gtest.h>

TEST(FNVHash, u32)
{
	EXPECT_EQ(FNV1aHash32(""), 2166136261u);

	/* zero-hash challenge results from
	   http://www.isthe.com/chongo/tech/comp/fnv/ */
	EXPECT_EQ(FNV1aHash32("\xcc\x24\x31\xc4"), 0);
	EXPECT_EQ(FNV1aHash32("\xe0\x4d\x9f\xcb"), 0);

	/* from IETF draft-eastlake-fnv Appendix C: A Few Test Vectors */
	EXPECT_EQ(FNV1aHash32("a"), 0xe40c292c);
	EXPECT_EQ(FNV1aHash32("foobar"), 0xbf9cf968);
}

TEST(FNVHash, u64)
{
	EXPECT_EQ(FNV1aHash64(""), 14695981039346656037u);

	/* zero-hash challenge results from
	   http://www.isthe.com/chongo/tech/comp/fnv/ */
	EXPECT_EQ(FNV1aHash64("\xd5\x6b\xb9\x53\x42\x87\x08\x36"), 0);

	/* from IETF draft-eastlake-fnv Appendix C: A Few Test Vectors */
	EXPECT_EQ(FNV1aHash64("a"), 0xaf63dc4c8601ec8c);
	EXPECT_EQ(FNV1aHash64("foobar"), 0x85944171f73967e8);
}
