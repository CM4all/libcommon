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

#ifndef PREPARED_CHILD_PROCESS_HXX
#define PREPARED_CHILD_PROCESS_HXX

#include "util/StaticArray.hxx"
#include "CgroupOptions.hxx"
#include "ResourceLimits.hxx"
#include "RefenceOptions.hxx"
#include "NamespaceOptions.hxx"
#include "UidGid.hxx"

#include <string>
#include <forward_list>

#include <assert.h>

class UniqueFileDescriptor;
class UniqueSocketDescriptor;
template<typename T> struct ConstBuffer;

struct PreparedChildProcess {
    /**
     * An opaque string which may be used by SpawnHook methods.  For
     * example, it may be a template name.
     */
    const char *hook_info = nullptr;

    /**
     * A function pointer which will be called instead of executing a
     * new program with execve().
     *
     * @return the process exit status
     */
    int (*exec_function)(PreparedChildProcess &&p) = nullptr;

    /**
     * This program will be executed (unless #exec_function is set).
     * If this is nullptr, then args.front() will be used.
     */
    const char *exec_path = nullptr;

    /**
     * An absolute path where STDERR output will be appended.  This
     * file will be opened after jailing and after applying the
     * #UidGid.
     */
    const char *stderr_path = nullptr;

    StaticArray<const char *, 32> args;
    StaticArray<const char *, 32> env;
    int stdin_fd = -1, stdout_fd = -1, stderr_fd = -1, control_fd = -1;

    /**
     * The umask for the new child process.  -1 means do not change
     * it.
     */
    int umask = -1;

    /**
     * The CPU scheduler priority configured with setpriority(),
     * ranging from -20 to 19.
     */
    int priority = 0;

    CgroupOptions cgroup;

    RefenceOptions refence;

    NamespaceOptions ns;

    ResourceLimits rlimits;

    UidGid uid_gid;

    /**
     * Change to this new root directory.  This feature should not be
     * used; use NamespaceOptions::pivot_root instead.  It is only
     * here for compatibility.
     */
    const char *chroot = nullptr;

    /**
     * Change the working directory.
     */
    const char *chdir = nullptr;

    /**
     * Select the "idle" CPU scheduling policy.  With this policy, the
     * "priority" value is ignored.
     *
     * @see sched(7)
     */
    bool sched_idle = false;

    /**
     * Select the "idle" I/O scheduling class.
     *
     * @see ioprio_set(2)
     */
    bool ioprio_idle = false;

    bool forbid_user_ns = false;

    bool forbid_multicast = false;

    bool forbid_bind = false;

    bool no_new_privs = false;

    /**
     * Make #stdin_fd and #stdout_fd (which must be equal) the
     * controlling TTY?
     */
    bool tty = false;

    /**
     * String allocations for SetEnv().
     */
    std::forward_list<std::string> strings;

    PreparedChildProcess();
    ~PreparedChildProcess();

    PreparedChildProcess(const PreparedChildProcess &) = delete;
    PreparedChildProcess &operator=(const PreparedChildProcess &) = delete;

    /**
     * Is at least one system call filter option enabled?  If yes,
     * then failures to set up the filter are fatal.
     */
    bool HasSyscallFilter() const {
        return forbid_user_ns || forbid_multicast || forbid_bind;
    }

    /**
     * @return true on success, false if the #args array is full
     */
    bool InsertWrapper(ConstBuffer<const char *> w);

    /**
     * @return true on success, false if the #args array is full
     */
    bool Append(const char *arg) {
        assert(arg != nullptr);

        if (args.size() + 1 >= args.capacity())
            return false;

        args.push_back(arg);
        return true;
    }

    /**
     * @return true on success, false if the #env array is full
     */
    bool PutEnv(const char *p) {
        if (env.size() + 1 >= env.capacity())
            return false;

        env.push_back(p);
        return true;
    }

    /**
     * @return true on success, false if the #env array is full
     */
    bool SetEnv(const char *name, const char *value);

    void SetStdin(int fd);
    void SetStdout(int fd);
    void SetStderr(int fd);
    void SetControl(int fd);

    void SetStdin(UniqueFileDescriptor &&fd);
    void SetStdout(UniqueFileDescriptor &&fd);
    void SetStderr(UniqueFileDescriptor &&fd);
    void SetControl(UniqueFileDescriptor &&fd);

    void SetStdin(UniqueSocketDescriptor &&fd);
    void SetStdout(UniqueSocketDescriptor &&fd);
    void SetStderr(UniqueSocketDescriptor &&fd);
    void SetControl(UniqueSocketDescriptor &&fd);

    /**
     * Finish this object and return the executable path.
     */
    const char *Finish();
};

#endif
