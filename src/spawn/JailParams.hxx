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

#ifndef BENG_PROXY_JAIL_PARAMS_HXX
#define BENG_PROXY_JAIL_PARAMS_HXX

#include "translation/Features.hxx"

struct PreparedChildProcess;
class AllocatorPtr;
class MatchInfo;

struct JailParams {
    bool enabled = false;

#if TRANSLATION_ENABLE_EXPAND
    bool expand_home_directory = false;
#endif

    const char *account_id = nullptr;
    const char *site_id = nullptr;
    const char *user_name = nullptr;
    const char *host_name = nullptr;
    const char *home_directory = nullptr;

    JailParams() = default;
    JailParams(AllocatorPtr alloc, const JailParams &src);

    /**
     * Throws std::runtime_error on error.
     */
    void Check() const;

    char *MakeId(char *p) const;

    bool InsertWrapper(PreparedChildProcess &p,
                       const char *document_root) const;

#if TRANSLATION_ENABLE_EXPAND
    bool IsExpandable() const {
        return expand_home_directory;
    }

    void Expand(AllocatorPtr alloc, const MatchInfo &match_info);
#endif
};

#endif
