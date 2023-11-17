// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PidfdEvent.hxx"
#include "ExitListener.hxx"
#include "event/Loop.hxx"
#include "time/Convert.hxx"
#include "system/PidFD.h"
#include "io/UniqueFileDescriptor.hxx"

#include <errno.h>
#include <string.h> // for strerror()
#include <sys/resource.h> // for struct rusage
#include <sys/wait.h>

#if defined(__GLIBC__) && __GLIBC__+0 == 2 && __GLIBC_MINOR__+0 < 36
#include <linux/wait.h> // for P_PIDFD on glibc < 2.36
#endif

using std::string_view_literals::operator""sv;

/**
 * A custom waitid() system call wrapper which, unlike the glibc
 * wrapper, supports the "rusage" parameter.
 */
static inline int
my_waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options,
	  struct rusage *rusage) noexcept
{
	return syscall(__NR_waitid, idtype, id, infop, options, rusage);
}

PidfdEvent::PidfdEvent(EventLoop &event_loop,
		       UniqueFileDescriptor &&_pidfd,
		       const char *_name,
		       ExitListener &_listener) noexcept
	:logger(_name),
	 start_time(event_loop.SteadyNow()),
	 event(event_loop, BIND_THIS_METHOD(OnPidfdReady),
	       _pidfd.Release()),
	 listener(&_listener)
{
	event.ScheduleRead();
}

PidfdEvent::~PidfdEvent() noexcept
{
	event.Close();
}

inline void
PidfdEvent::OnPidfdReady(unsigned) noexcept
{
	assert(IsDefined());

	siginfo_t info;
	info.si_pid = 0;

	struct rusage rusage;
	if (my_waitid((idtype_t)P_PIDFD, event.GetFileDescriptor().Get(),
		      &info, WEXITED|WNOHANG, &rusage) < 0) {
		/* errno==ECHILD can happen if the child has exited
		   while ZombieReaper was already running (because
		   many child processes have exited at the same time)
		   - pretend the child has exited */
		const int e = errno;
		logger(3, "waitid() failed: ", strerror(e));
		event.Close();
		listener->OnChildProcessExit(-e);
		return;
	}

	if (info.si_pid == 0)
		return;

	int status = W_EXITCODE(info.si_status, 0);

	switch (info.si_code) {
	case CLD_EXITED:
		if (info.si_status == 0)
			logger(5, "exited with success");
		else
			logger(2, "exited with status ", info.si_status);
		break;

	case CLD_KILLED:
		status = W_STOPCODE(info.si_status);

		logger(info.si_status == SIGTERM ? 4 : 1,
		       "died from signal ", info.si_status);
		break;

	case CLD_DUMPED:
		status = W_STOPCODE(info.si_status) | WCOREFLAG;

		logger(1, "died from signal ", info.si_status,
		       " (core dumped)");
		break;

	case CLD_STOPPED:
	case CLD_TRAPPED:
	case CLD_CONTINUED:
		return;
	}

	const auto duration = GetEventLoop().SteadyNow() - start_time;
	const auto duration_f = std::chrono::duration_cast<std::chrono::duration<double>>(duration);
	const auto utime = ToSteadyClockDuration(rusage.ru_utime);
	const auto utime_f = std::chrono::duration_cast<std::chrono::duration<double>>(utime);
	const auto stime = ToSteadyClockDuration(rusage.ru_stime);
	const auto stime_f = std::chrono::duration_cast<std::chrono::duration<double>>(stime);

	logger.Fmt(6, "stats: {:1.3}s elapsed, {:1.3}s user, {:1.3}s sys, {}/{} faults, {}/{} switches"sv,
		   duration_f.count(),
		   utime_f.count(),
		   stime_f.count(),
		   rusage.ru_minflt, rusage.ru_majflt,
		   rusage.ru_nvcsw, rusage.ru_nivcsw);

	event.Close();

	listener->OnChildProcessExit(status);
}

bool
PidfdEvent::Kill(int signo) noexcept
{
	assert(IsDefined());

	if (my_pidfd_send_signal(event.GetFileDescriptor().Get(),
				 signo, nullptr, 0) < 0) {
		logger(1, "failed to kill child process: ", strerror(errno));
		return false;
	}

	return true;
}
