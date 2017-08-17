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

#include "ProcessName.hxx"

#include <assert.h>
#include <string.h>
#include <sys/prctl.h>

static struct {
    int argc;
    char **argv;
    size_t max_length;
} process_name;

void
InitProcessName(int argc, char **argv)
{
    assert(process_name.argc == 0);
    assert(process_name.argv == nullptr);
    assert(argc > 0);
    assert(argv != nullptr);
    assert(argv[0] != nullptr);

    process_name.argc = argc;
    process_name.argv = argv;
    process_name.max_length = strlen(argv[0]);
}

void
SetProcessName(const char *name)
{
    prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);

    if (process_name.argc > 0 && process_name.argv[0] != nullptr) {
        for (int i = 1; i < process_name.argc; ++i)
            if (process_name.argv[i] != nullptr)
                memset(process_name.argv[i], 0, strlen(process_name.argv[i]));

        strncpy(process_name.argv[0], name, process_name.max_length);
    }
}
