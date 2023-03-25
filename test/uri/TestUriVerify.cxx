/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "uri/Verify.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(UriVerify, VerifyDomainName)
{
	EXPECT_TRUE(VerifyDomainName("a"));
	EXPECT_TRUE(VerifyDomainName("A"));
	EXPECT_TRUE(VerifyDomainName("a-b"));
	EXPECT_TRUE(VerifyDomainName("a.b"));
	EXPECT_TRUE(VerifyDomainName("a.b.c.d.efghi.jkl"));
	EXPECT_FALSE(VerifyDomainName(""));
	EXPECT_FALSE(VerifyDomainName("-"));
	EXPECT_FALSE(VerifyDomainName("-b"));
	EXPECT_FALSE(VerifyDomainName("a-"));
	EXPECT_FALSE(VerifyDomainName("a:"));
	EXPECT_FALSE(VerifyDomainName("a:80"));
	EXPECT_TRUE(VerifyDomainName("a.a-b"));
	EXPECT_FALSE(VerifyDomainName("a.-b"));
	EXPECT_FALSE(VerifyDomainName("a..b"));
	EXPECT_FALSE(VerifyDomainName("a."));
	EXPECT_FALSE(VerifyDomainName(".b"));
	EXPECT_TRUE(VerifyDomainName("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890"));
	EXPECT_TRUE(VerifyDomainName("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890.abcdefghijklmnopqrstuvwxyz"));
	EXPECT_FALSE(VerifyDomainName("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890a"));
}

TEST(UriVerify, VerifyLowerDomainName)
{
	EXPECT_TRUE(VerifyLowerDomainName("a"));
	EXPECT_FALSE(VerifyLowerDomainName("A"));
	EXPECT_TRUE(VerifyLowerDomainName("a-b"));
	EXPECT_TRUE(VerifyLowerDomainName("a.b"));
	EXPECT_TRUE(VerifyLowerDomainName("a.b.c.d.efghi.jkl"));
}

TEST(UriVerify, VerifyUriHostPort)
{
	EXPECT_FALSE(VerifyUriHostPort(""));
	EXPECT_FALSE(VerifyUriHostPort(":80"));
	EXPECT_TRUE(VerifyUriHostPort("a"));
	EXPECT_TRUE(VerifyUriHostPort("a.b"));
	EXPECT_TRUE(VerifyUriHostPort("a.b:8080"));
	EXPECT_FALSE(VerifyUriHostPort("a.b:8080:1"));
	EXPECT_TRUE(VerifyUriHostPort("localhost"));
	EXPECT_TRUE(VerifyUriHostPort("localhost:80"));

	/* IPv4 */
	EXPECT_TRUE(VerifyUriHostPort("1.2.3.4:8080"));
	EXPECT_TRUE(VerifyUriHostPort("1.2.3.4:65535"));
	EXPECT_FALSE(VerifyUriHostPort("1.2.3.4:123456"));
	EXPECT_FALSE(VerifyUriHostPort("1.2.3.4:a"));
	EXPECT_FALSE(VerifyUriHostPort("1.2.3.4:1a2"));

	/* IPv6 */
	EXPECT_TRUE(VerifyUriHostPort("::"));
	EXPECT_TRUE(VerifyUriHostPort("::1"));
	EXPECT_TRUE(VerifyUriHostPort("2001::1"));
	EXPECT_FALSE(VerifyUriHostPort("20010::1"));
	EXPECT_TRUE(VerifyUriHostPort("abcd:ef::1"));
	EXPECT_FALSE(VerifyUriHostPort("abcd:efg::1"));
	EXPECT_TRUE(VerifyUriHostPort("[::1]:8080"));
	EXPECT_TRUE(VerifyUriHostPort("[::1]:65535"));
	EXPECT_FALSE(VerifyUriHostPort("[::1]:123456"));
	EXPECT_FALSE(VerifyUriHostPort("[::1]:a"));
}

TEST(UriVerify, Segment)
{
	EXPECT_TRUE(uri_segment_verify(""sv));
	EXPECT_TRUE(uri_segment_verify("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz%01234567890-.ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ_~!$&'()*+,;=:@"sv));

	EXPECT_FALSE(uri_segment_verify("/"sv));
	EXPECT_FALSE(uri_segment_verify("\0"sv));
	EXPECT_FALSE(uri_segment_verify("\""sv));
	EXPECT_FALSE(uri_segment_verify("`"sv));
	EXPECT_FALSE(uri_segment_verify("["sv));
	EXPECT_FALSE(uri_segment_verify("]"sv));
	EXPECT_FALSE(uri_segment_verify("{"sv));
	EXPECT_FALSE(uri_segment_verify("}"sv));
	EXPECT_FALSE(uri_segment_verify("?"sv));
	EXPECT_FALSE(uri_segment_verify("^"sv));
}

TEST(UriVerify, Path)
{
	EXPECT_FALSE(uri_path_verify(""sv));
	EXPECT_FALSE(uri_path_verify("a"sv));
	EXPECT_FALSE(uri_path_verify("*"sv));
	EXPECT_TRUE(uri_path_verify("/"sv));
	EXPECT_TRUE(uri_path_verify("//"sv));
	EXPECT_TRUE(uri_path_verify("///"sv));
	EXPECT_TRUE(uri_path_verify("///a"sv));
	EXPECT_TRUE(uri_path_verify("/a/a/a"sv));
	EXPECT_FALSE(uri_path_verify("/a/a/a?"sv));
}

TEST(UriVerify, Paranoid)
{
	EXPECT_TRUE(uri_path_verify_paranoid(""));
	EXPECT_TRUE(uri_path_verify_paranoid("/"));
	EXPECT_TRUE(uri_path_verify_paranoid(" "));
	EXPECT_FALSE(uri_path_verify_paranoid("."));
	EXPECT_FALSE(uri_path_verify_paranoid("./"));
	EXPECT_FALSE(uri_path_verify_paranoid("./foo"));
	EXPECT_FALSE(uri_path_verify_paranoid(".."));
	EXPECT_FALSE(uri_path_verify_paranoid("../"));
	EXPECT_FALSE(uri_path_verify_paranoid("../foo"));
	EXPECT_FALSE(uri_path_verify_paranoid(".%2e/foo"));
	EXPECT_TRUE(uri_path_verify_paranoid("foo/bar"));
	EXPECT_FALSE(uri_path_verify_paranoid("foo%2fbar"));
	EXPECT_FALSE(uri_path_verify_paranoid("/foo/bar?A%2fB%00C%"));
	EXPECT_FALSE(uri_path_verify_paranoid("foo/./bar"));
	EXPECT_TRUE(uri_path_verify_paranoid("foo//bar"));
	EXPECT_FALSE(uri_path_verify_paranoid("foo/%2ebar"));
	EXPECT_FALSE(uri_path_verify_paranoid("foo/.%2e/bar"));
	EXPECT_FALSE(uri_path_verify_paranoid("foo/.%2e"));
	EXPECT_FALSE(uri_path_verify_paranoid("foo/bar/.."));
	EXPECT_FALSE(uri_path_verify_paranoid("foo/bar/../bar"));
	EXPECT_FALSE(uri_path_verify_paranoid("f%00"));
	EXPECT_TRUE(uri_path_verify_paranoid("f%20"));
	EXPECT_TRUE(uri_path_verify_paranoid("index%2ehtml"));
}

TEST(UriVerify, Query)
{
	EXPECT_TRUE(VerifyUriQuery(""sv));
	EXPECT_TRUE(VerifyUriQuery("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz%01234567890-.ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ_~!$&'()*+,;=:@?/"sv));

	EXPECT_FALSE(VerifyUriQuery("\0"sv));
	EXPECT_FALSE(VerifyUriQuery("\""sv));
	EXPECT_FALSE(VerifyUriQuery("`"sv));
	EXPECT_FALSE(VerifyUriQuery("["sv));
	EXPECT_FALSE(VerifyUriQuery("]"sv));
	EXPECT_FALSE(VerifyUriQuery("{"sv));
	EXPECT_FALSE(VerifyUriQuery("}"sv));
	EXPECT_FALSE(VerifyUriQuery("^"sv));
}

TEST(UriVerify, VerifyHttpUrl)
{
	EXPECT_FALSE(VerifyHttpUrl(""));
	EXPECT_FALSE(VerifyHttpUrl("http://"));
	EXPECT_FALSE(VerifyHttpUrl("http:///"));
	EXPECT_FALSE(VerifyHttpUrl("http://a"));
	EXPECT_TRUE(VerifyHttpUrl("http://a/"));
	EXPECT_TRUE(VerifyHttpUrl("http://a/b/c/"));
	EXPECT_TRUE(VerifyHttpUrl("http://a/b/c/?"));
	EXPECT_TRUE(VerifyHttpUrl("http://a/b/c/?d"));
	EXPECT_FALSE(VerifyHttpUrl("http://a/b/c/?d\""));
	EXPECT_FALSE(VerifyHttpUrl("http://a/b/c/#"));
	EXPECT_TRUE(VerifyHttpUrl("http://[1234::5678]/"));
	EXPECT_TRUE(VerifyHttpUrl("http://[1234::5678]:80/"));
	EXPECT_TRUE(VerifyHttpUrl("http://foo.example.com/"));
}
