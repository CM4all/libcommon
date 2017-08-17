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

#include "Edit.hxx"
#include "Unique.hxx"
#include "GeneralName.hxx"
#include "Error.hxx"

#include "openssl/x509v3.h"

static UniqueX509_EXTENSION
MakeExt(int nid, const char *value)
{
    UniqueX509_EXTENSION ext(X509V3_EXT_conf_nid(nullptr, nullptr, nid,
                                                 const_cast<char *>(value)));
    if (ext == nullptr)
        throw SslError("X509V3_EXT_conf_nid() failed");

    return ext;
}

void
AddExt(X509 &cert, int nid, const char *value)
{
    X509_add_ext(&cert, MakeExt(nid, value).get(), -1);
}

void
AddAltNames(X509_REQ &req, OpenSSL::GeneralNames gn)
{
    UniqueX509_EXTENSIONS sk(sk_X509_EXTENSION_new_null());
    sk_X509_EXTENSION_push(sk.get(),
                           X509V3_EXT_i2d(NID_subject_alt_name, 0, gn.get()));

    X509_REQ_add_extensions(&req, sk.get());
}
