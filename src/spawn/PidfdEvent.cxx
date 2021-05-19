/*
 * Copyright 2021 CM4all GmbH
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

#include "PidfdEvent.hxx"
#include "ExitListener.hxx"
#include "system/PidFD.h"
#include "io/UniqueFileDescriptor.hxx"

#include <linux/wait.h>
#include <sys/wait.h>

PidfdEvent::PidfdEvent(EventLoop &event_loop,
		       UniqueFileDescriptor &&_pidfd,
		       const char *_name,
		       ExitListener &_listener) noexcept
	:logger(_name),
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

/* TODO
static constexpr double
timeval_to_double(const struct timeval &tv) noexcept
{
	return tv.tv_sec + tv.tv_usec / 1000000.;
}
*/

inline void
PidfdEvent::OnPidfdReady(unsigned) noexcept
{
	assert(IsDefined());

	siginfo_t info;
	info.si_pid = 0;

	// TODO use the "struct rusage" parameter

	/* the __WALL is necessary because we clone() without
	   SIGCHLD */
	if (waitid((idtype_t)P_PIDFD, event.GetFileDescriptor().Get(),
		   &info, WEXITED|WNOHANG|__WALL) < 0) {
		/* should not happen ... */
		logger(1, "waitid() failed: ", strerror(errno));
		event.Cancel();
		return;
	}

	if (info.si_pid == 0)
		return;

	switch (info.si_code) {
	case CLD_EXITED:
		if (info.si_status == 0)
			logger(5, "exited with success");
		else
			logger(2, "exited with status ", info.si_status);
		break;

	case CLD_KILLED:
		logger(info.si_status == SIGTERM ? 4 : 1,
		       "died from signal ", info.si_status);
		break;

	case CLD_DUMPED:
		logger(1, "died from signal ", info.si_status,
		       " (core dumped)");
		break;

	case CLD_STOPPED:
	case CLD_TRAPPED:
	case CLD_CONTINUED:
		return;
	}

	/* TODO
	const auto duration = GetEventLoop().SteadyNow() - start_time;
	const auto duration_f = std::chrono::duration_cast<std::chrono::duration<double>>(duration);

	logger.Format(6, "stats: %1.3fs elapsed, %1.3fs user, %1.3fs sys, %ld/%ld faults, %ld/%ld switches",
		      duration_f.count(),
		      timeval_to_double(rusage.ru_utime),
		      timeval_to_double(rusage.ru_stime),
		      rusage.ru_minflt, rusage.ru_majflt,
		      rusage.ru_nvcsw, rusage.ru_nivcsw);
	*/

	event.Close();

	listener->OnChildProcessExit(info.si_status);
}

bool
PidfdEvent::Kill(int signo) noexcept
{
	assert(IsDefined());

	if (int error = my_pidfd_send_signal(event.GetFileDescriptor().Get(),
					     signo, nullptr, 0);
	    error < 0) {
		logger(1, "failed to kill child process: ", strerror(-error));
		return false;
	}

	return true;
}
