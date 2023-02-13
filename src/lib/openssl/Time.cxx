// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Time.hxx"
#include "MemBio.hxx"

#include <openssl/asn1.h>

AllocatedString
FormatTime(const ASN1_TIME &t)
{
	return BioWriterToString([&t](BIO &bio){
			ASN1_TIME_print(&bio, &t);
		});
}

AllocatedString
FormatTime(const ASN1_TIME *t)
{
	if (t == nullptr)
		return nullptr;

	return FormatTime(*t);
}
