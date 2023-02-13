// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "uri/Verify.hxx"

#include <gtest/gtest.h>

TEST(UriVerifyTest, VerifyDomainName)
{
	ASSERT_TRUE(VerifyDomainName("a"));
	ASSERT_TRUE(VerifyDomainName("A"));
	ASSERT_TRUE(VerifyDomainName("a-b"));
	ASSERT_TRUE(VerifyDomainName("a.b"));
	ASSERT_TRUE(VerifyDomainName("a.b.c.d.efghi.jkl"));
	ASSERT_FALSE(VerifyDomainName(""));
	ASSERT_FALSE(VerifyDomainName("-"));
	ASSERT_FALSE(VerifyDomainName("-b"));
	ASSERT_FALSE(VerifyDomainName("a-"));
	ASSERT_FALSE(VerifyDomainName("a:"));
	ASSERT_FALSE(VerifyDomainName("a:80"));
	ASSERT_TRUE(VerifyDomainName("a.a-b"));
	ASSERT_FALSE(VerifyDomainName("a.-b"));
	ASSERT_FALSE(VerifyDomainName("a..b"));
	ASSERT_FALSE(VerifyDomainName("a."));
	ASSERT_FALSE(VerifyDomainName(".b"));
	ASSERT_TRUE(VerifyDomainName("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890"));
	ASSERT_TRUE(VerifyDomainName("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890.abcdefghijklmnopqrstuvwxyz"));
	ASSERT_FALSE(VerifyDomainName("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890a"));
}

TEST(UriVerifyTest, VerifyLowerDomainName)
{
	ASSERT_TRUE(VerifyLowerDomainName("a"));
	ASSERT_FALSE(VerifyLowerDomainName("A"));
	ASSERT_TRUE(VerifyLowerDomainName("a-b"));
	ASSERT_TRUE(VerifyLowerDomainName("a.b"));
	ASSERT_TRUE(VerifyLowerDomainName("a.b.c.d.efghi.jkl"));
}

TEST(UriVerifyTest, VerifyUriHostPort)
{
	ASSERT_FALSE(VerifyUriHostPort(""));
	ASSERT_FALSE(VerifyUriHostPort(":80"));
	ASSERT_TRUE(VerifyUriHostPort("a"));
	ASSERT_TRUE(VerifyUriHostPort("a.b"));
	ASSERT_TRUE(VerifyUriHostPort("a.b:8080"));
	ASSERT_FALSE(VerifyUriHostPort("a.b:8080:1"));
	ASSERT_TRUE(VerifyUriHostPort("localhost"));
	ASSERT_TRUE(VerifyUriHostPort("localhost:80"));

	/* IPv4 */
	ASSERT_TRUE(VerifyUriHostPort("1.2.3.4:8080"));
	ASSERT_TRUE(VerifyUriHostPort("1.2.3.4:65535"));
	ASSERT_FALSE(VerifyUriHostPort("1.2.3.4:123456"));
	ASSERT_FALSE(VerifyUriHostPort("1.2.3.4:a"));
	ASSERT_FALSE(VerifyUriHostPort("1.2.3.4:1a2"));

	/* IPv6 */
	ASSERT_TRUE(VerifyUriHostPort("::"));
	ASSERT_TRUE(VerifyUriHostPort("::1"));
	ASSERT_TRUE(VerifyUriHostPort("2001::1"));
	ASSERT_FALSE(VerifyUriHostPort("20010::1"));
	ASSERT_TRUE(VerifyUriHostPort("abcd:ef::1"));
	ASSERT_FALSE(VerifyUriHostPort("abcd:efg::1"));
	ASSERT_TRUE(VerifyUriHostPort("[::1]:8080"));
	ASSERT_TRUE(VerifyUriHostPort("[::1]:65535"));
	ASSERT_FALSE(VerifyUriHostPort("[::1]:123456"));
	ASSERT_FALSE(VerifyUriHostPort("[::1]:a"));
}

TEST(UriVerifyTest, Paranoid)
{
	ASSERT_TRUE(uri_path_verify_paranoid(""));
	ASSERT_TRUE(uri_path_verify_paranoid("/"));
	ASSERT_TRUE(uri_path_verify_paranoid(" "));
	ASSERT_FALSE(uri_path_verify_paranoid("."));
	ASSERT_FALSE(uri_path_verify_paranoid("./"));
	ASSERT_FALSE(uri_path_verify_paranoid("./foo"));
	ASSERT_FALSE(uri_path_verify_paranoid(".."));
	ASSERT_FALSE(uri_path_verify_paranoid("../"));
	ASSERT_FALSE(uri_path_verify_paranoid("../foo"));
	ASSERT_FALSE(uri_path_verify_paranoid(".%2e/foo"));
	ASSERT_TRUE(uri_path_verify_paranoid("foo/bar"));
	ASSERT_FALSE(uri_path_verify_paranoid("foo%2fbar"));
	ASSERT_FALSE(uri_path_verify_paranoid("/foo/bar?A%2fB%00C%"));
	ASSERT_FALSE(uri_path_verify_paranoid("foo/./bar"));
	ASSERT_TRUE(uri_path_verify_paranoid("foo//bar"));
	ASSERT_FALSE(uri_path_verify_paranoid("foo/%2ebar"));
	ASSERT_FALSE(uri_path_verify_paranoid("foo/.%2e/bar"));
	ASSERT_FALSE(uri_path_verify_paranoid("foo/.%2e"));
	ASSERT_FALSE(uri_path_verify_paranoid("foo/bar/.."));
	ASSERT_FALSE(uri_path_verify_paranoid("foo/bar/../bar"));
	ASSERT_FALSE(uri_path_verify_paranoid("f%00"));
	ASSERT_TRUE(uri_path_verify_paranoid("f%20"));
	ASSERT_TRUE(uri_path_verify_paranoid("index%2ehtml"));
}

TEST(UriVerifyTest, VerifyHttpUrl)
{
	ASSERT_FALSE(VerifyHttpUrl(""));
	ASSERT_FALSE(VerifyHttpUrl("http://"));
	ASSERT_FALSE(VerifyHttpUrl("http:///"));
	ASSERT_FALSE(VerifyHttpUrl("http://a"));
	ASSERT_TRUE(VerifyHttpUrl("http://a/"));
	ASSERT_TRUE(VerifyHttpUrl("http://a/b/c/"));
	ASSERT_TRUE(VerifyHttpUrl("http://a/b/c/?"));
	ASSERT_TRUE(VerifyHttpUrl("http://a/b/c/?d"));
	ASSERT_FALSE(VerifyHttpUrl("http://a/b/c/?d\""));
	ASSERT_FALSE(VerifyHttpUrl("http://a/b/c/#"));
	ASSERT_TRUE(VerifyHttpUrl("http://[1234::5678]/"));
	ASSERT_TRUE(VerifyHttpUrl("http://[1234::5678]:80/"));
	ASSERT_TRUE(VerifyHttpUrl("http://foo.example.com/"));
}
