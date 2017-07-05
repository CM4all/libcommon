/*
 * OpenSSL utilities.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SSL_DUMMY_HXX
#define SSL_DUMMY_HXX

#include "Unique.hxx"

UniqueX509
MakeSelfIssuedDummyCert(const char *common_name);

UniqueX509
MakeSelfSignedDummyCert(EVP_PKEY &key, const char *common_name);

#endif
