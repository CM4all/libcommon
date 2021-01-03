/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "Name.hxx"
#include "MemBio.hxx"
#include "Unique.hxx"

AllocatedString<>
ToString(X509_NAME *name)
{
	if (name == nullptr)
		return nullptr;

	return BioWriterToString([name](BIO &bio){
			X509_NAME_print_ex(&bio, name, 0,
					   ASN1_STRFLGS_UTF8_CONVERT | XN_FLAG_SEP_COMMA_PLUS);
		});
}

AllocatedString<>
NidToString(X509_NAME &name, int nid)
{
	char buffer[1024];
	int len = X509_NAME_get_text_by_NID(&name, nid, buffer, sizeof(buffer));
	if (len < 0)
		return nullptr;

	return AllocatedString<>::Duplicate({buffer, std::size_t(len)});
}

static AllocatedString<>
GetCommonName(X509_NAME &name)
{
	return NidToString(name, NID_commonName);
}

AllocatedString<>
GetCommonName(X509 &cert)
{
	X509_NAME *subject = X509_get_subject_name(&cert);
	return subject != nullptr
		? GetCommonName(*subject)
		: nullptr;
}

AllocatedString<>
GetIssuerCommonName(X509 &cert)
{
	X509_NAME *subject = X509_get_issuer_name(&cert);
	return subject != nullptr
		? GetCommonName(*subject)
		: nullptr;
}
