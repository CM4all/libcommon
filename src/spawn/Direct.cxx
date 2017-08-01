/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Direct.hxx"
#include "Prepared.hxx"
#include "SeccompFilter.hxx"
#include "SyscallFilter.hxx"
#include "Init.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "system/IOPrio.hxx"
#include "util/PrintException.hxx"

#include "util/Compiler.h"

#include <systemd/sd-journal.h>

#include <exception>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

static void
CheckedDup2(FileDescriptor oldfd, FileDescriptor newfd)
{
    if (oldfd.IsDefined())
        oldfd.CheckDuplicate(newfd);
}

static void
CheckedDup2(int oldfd, int newfd)
{
    CheckedDup2(FileDescriptor(oldfd), FileDescriptor(newfd));
}

static void
DisconnectTty()
{
    FileDescriptor fd;
    if (fd.Open("/dev/tty", O_RDWR)) {
        (void) ioctl(fd.Get(), TIOCNOTTY, nullptr);
        fd.Close();
    }
}

static void
UnignoreSignals()
{
    /* restore all signals which were set to SIG_IGN by
       RunSpawnServer2() and others */
    static constexpr int signals[] = {
        SIGHUP,
        SIGINT, SIGQUIT,
        SIGPIPE,
        SIGTERM,
        SIGUSR1, SIGUSR2,
        SIGCHLD,
        SIGTRAP,
    };

    for (auto i : signals)
        signal(i, SIG_DFL);
}

static void
UnblockSignals()
{
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_UNBLOCK, &mask, nullptr);
}

gcc_noreturn
static void
Exec(const char *path, PreparedChildProcess &&p,
     const CgroupState &cgroup_state)
try {
    UnignoreSignals();
    UnblockSignals();

    if (p.umask >= 0)
        umask(p.umask);

    int stdout_fd = p.stdout_fd, stderr_fd = p.stderr_fd;
    if (stdout_fd < 0 || (stderr_fd < 0 && p.stderr_path == nullptr)) {
        /* if no log destination was specified, log to the systemd
           journal */
        /* note: this must be done before NamespaceOptions::Setup(),
           because inside the new root, we don't have access to
           /run/systemd/journal/stdout */
        int journal_fd = sd_journal_stream_fd(p.args.front(), LOG_INFO, true);
        if (stdout_fd < 0)
            stdout_fd = journal_fd;
        if (stderr_fd < 0 && p.stderr_path == nullptr)
            stderr_fd = journal_fd;
    }

    p.cgroup.Apply(cgroup_state);

    if (p.ns.enable_cgroup && p.cgroup.IsDefined()) {
        /* if the process was just moved to another cgroup, we need to
           unshare the cgroup namespace again to hide our new cgroup
           membership */

#ifndef CLONE_NEWCGROUP
        constexpr int CLONE_NEWCGROUP = 0x02000000;
#endif

        if (unshare(CLONE_NEWCGROUP) < 0)
            throw MakeErrno("Failed to unshare cgroup namespace");
    }

    p.refence.Apply();

    p.ns.Setup(p.uid_gid);
    p.rlimits.Apply();

    if (p.chroot != nullptr && chroot(p.chroot) < 0) {
        fprintf(stderr, "chroot('%s') failed: %s\n",
                p.chroot, strerror(errno));
        _exit(EXIT_FAILURE);
    }

    if (p.chdir != nullptr && chdir(p.chdir) < 0) {
        fprintf(stderr, "chdir('%s') failed: %s\n",
                p.chdir, strerror(errno));
        _exit(EXIT_FAILURE);
    }

    if (p.sched_idle) {
        static struct sched_param sched_param;
        sched_setscheduler(0, SCHED_IDLE, &sched_param);
    }

    if (p.priority != 0 &&
        setpriority(PRIO_PROCESS, getpid(), p.priority) < 0) {
        fprintf(stderr, "setpriority() failed: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    }

    if (p.ioprio_idle)
        ioprio_set_idle();

    if (p.ns.enable_pid) {
        setsid();

        const auto pid = SpawnInitFork();
        assert(pid >= 0);

        if (pid > 0)
            _exit(SpawnInit(pid));
    }

    if (!p.uid_gid.IsEmpty())
        p.uid_gid.Apply();

    if (p.no_new_privs)
        prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

    if (stderr_fd < 0 && p.stderr_path != nullptr) {
        stderr_fd = open(p.stderr_path,
                         O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC|O_NOCTTY,
                         0600);
        if (stderr_fd < 0) {
            perror("Failed to open STDERR_PATH");
            _exit(EXIT_FAILURE);
        }
    }

    constexpr int CONTROL_FILENO = 3;
    CheckedDup2(p.stdin_fd, STDIN_FILENO);
    CheckedDup2(stdout_fd, STDOUT_FILENO);
    CheckedDup2(stderr_fd, STDERR_FILENO);
    CheckedDup2(p.control_fd, CONTROL_FILENO);

    if (p.tty)
        DisconnectTty();

    setsid();

    if (p.tty) {
        assert(p.stdin_fd >= 0);
        assert(p.stdin_fd == p.stdout_fd);

        if (ioctl(p.stdin_fd, TIOCSCTTY, nullptr) < 0) {
            perror("Failed to set the controlling terminal");
            _exit(EXIT_FAILURE);
        }
    }

    try {
        Seccomp::Filter sf(SCMP_ACT_ALLOW);
        sf.AddSecondaryArchs();

        BuildSyscallFilter(sf);

        if (p.forbid_user_ns)
            ForbidUserNamespace(sf);

        if (p.forbid_multicast)
            ForbidMulticast(sf);

        if (p.forbid_bind)
            ForbidBind(sf);

        sf.Load();
    } catch (const std::runtime_error &e) {
        if (p.HasSyscallFilter())
            /* filter options have been explicitly enabled, and thus
               failure to set up the filter are fatal */
            throw;

        fprintf(stderr, "Failed to setup seccomp filter for '%s': %s\n",
                path, e.what());
    }

    if (p.exec_function != nullptr) {
        _exit(p.exec_function(std::move(p)));
    } else {
        execve(path, const_cast<char *const*>(p.args.raw()),
               const_cast<char *const*>(p.env.raw()));

        fprintf(stderr, "failed to execute %s: %s\n", path, strerror(errno));
        _exit(EXIT_FAILURE);
    }
} catch (const std::exception &e) {
    PrintException(e);
    _exit(EXIT_FAILURE);
}

struct SpawnChildProcessContext {
    PreparedChildProcess &&params;
    const CgroupState &cgroup_state;

    const char *path;

    /**
     * A pipe used by the child process to wait for the parent to set
     * it up (e.g. uid/gid mappings).
     */
    UniqueFileDescriptor wait_pipe_r, wait_pipe_w;

    SpawnChildProcessContext(PreparedChildProcess &&_params,
                             const CgroupState &_cgroup_state)
        :params(std::move(_params)),
         cgroup_state(_cgroup_state),
         path(_params.Finish()) {}
};

static int
spawn_fn(void *_ctx)
{
    auto &ctx = *(SpawnChildProcessContext *)_ctx;

    if (ctx.wait_pipe_r.IsDefined()) {
        /* wait for the parent to set us up */

        ctx.wait_pipe_w.Close();

        /* expect one byte to indicate success, and then the pipe will
           be closed by the parent */
        char buffer;
        if (ctx.wait_pipe_r.Read(&buffer, sizeof(buffer)) != 1 ||
            ctx.wait_pipe_r.Read(&buffer, sizeof(buffer)) != 0)
            _exit(EXIT_FAILURE);
    }

    Exec(ctx.path, std::move(ctx.params), ctx.cgroup_state);
}

pid_t
SpawnChildProcess(PreparedChildProcess &&params,
                  const CgroupState &cgroup_state)
{
    int clone_flags = SIGCHLD;
    clone_flags = params.ns.GetCloneFlags(clone_flags);

    SpawnChildProcessContext ctx(std::move(params), cgroup_state);

    if (params.ns.enable_user && geteuid() == 0) {
        /* we'll set up the uid/gid mapping from the privileged
           parent; create a pipe to synchronize this with the child */

        if (!UniqueFileDescriptor::CreatePipe(ctx.wait_pipe_r, ctx.wait_pipe_w))
            throw MakeErrno("pipe() failed");

        /* don't do it again inside the child process by disabling the
           feature on the child's copy */
        ctx.params.ns.enable_user = false;
    }

    char stack[8192];
    long pid = clone(spawn_fn, stack + sizeof(stack), clone_flags, &ctx);
    if (pid < 0)
        throw MakeErrno("clone() failed");

    if (ctx.wait_pipe_w.IsDefined()) {
        /* set up the child's uid/gid mapping and wake it up */
        ctx.wait_pipe_r.Close();
        ctx.params.ns.SetupUidGidMap(ctx.params.uid_gid, pid);

        /* after success (no exception was thrown), we send one byte
           to the pipe and close it, so the child knows everything's
           ok; without that byte, it would exit immediately */
        static constexpr char buffer = 0;
        ctx.wait_pipe_w.Write(&buffer, sizeof(buffer));
        ctx.wait_pipe_w.Close();
    }

    return pid;
}
