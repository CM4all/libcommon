/*
 * Copyright 2007-2017 Content Management AG
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

#include "AltName.hxx"
#include "GeneralName.hxx"

static void
FillNameList(std::forward_list<std::string> &list, OpenSSL::GeneralNames src)
{
	for (const auto &name : src) {
		if (name.GetType() == GEN_DNS) {
			const auto dns_name = name.GetDnsName();
			if (dns_name == nullptr)
				continue;

			list.emplace_front(dns_name.data, dns_name.size);
		}
	}
}

gcc_pure
std::forward_list<std::string>
GetSubjectAltNames(X509 &cert)
{
	std::forward_list<std::string> list;

	for (int i = -1;
	     (i = X509_get_ext_by_NID(&cert, NID_subject_alt_name, i)) >= 0;) {
		auto ext = X509_get_ext(&cert, i);
		if (ext == nullptr)
			continue;

		OpenSSL::UniqueGeneralNames gn(reinterpret_cast<GENERAL_NAMES *>(X509V3_EXT_d2i(ext)));
		if (!gn)
			continue;

		FillNameList(list, gn);
	}

	return list;
}
