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

#pragma once

#include "io/Logger.hxx"
#include "event/TimerEvent.hxx"
#include "event/SignalEvent.hxx"

#include "util/Compiler.h"

#include <boost/intrusive/set.hpp>

#include <string>
#include <chrono>

#include <assert.h>
#include <sys/types.h>

class ExitListener;

/**
 * Multiplexer for SIGCHLD.
 */
class ChildProcessRegistry {

	struct ChildProcess
		: boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> {

		const Logger logger;

		const pid_t pid;

		const std::string name;

		/**
		 * The time when this child process was started (registered in
		 * this library).
		 */
		const std::chrono::steady_clock::time_point start_time;

		ExitListener *listener;

		/**
		 * This timer is set up by child_kill_signal().  If the child
		 * process hasn't exited after a certain amount of time, we send
		 * SIGKILL.
		 */
		TimerEvent kill_timeout_event;

		ChildProcess(EventLoop &event_loop,
			     pid_t _pid, const char *_name,
			     ExitListener *_listener) noexcept;

		auto &GetEventLoop() noexcept {
			return kill_timeout_event.GetEventLoop();
		}

		void Disable() noexcept {
			kill_timeout_event.Cancel();
		}

		void OnExit(int status, const struct rusage &rusage) noexcept;

		void KillTimeoutCallback() noexcept;

		struct Compare {
			bool operator()(const ChildProcess &a,
					const ChildProcess &b) const noexcept {
				return a.pid < b.pid;
			}

			bool operator()(const ChildProcess &a,
					pid_t b) const noexcept {
				return a.pid < b;
			}

			bool operator()(pid_t a,
					const ChildProcess &b) const noexcept {
				return a < b.pid;
			}
		};
	};

	const LLogger logger;

	EventLoop &event_loop;

	typedef boost::intrusive::set<ChildProcess,
				      boost::intrusive::compare<ChildProcess::Compare>,
				      boost::intrusive::constant_time_size<true>> ChildProcessSet;

	ChildProcessSet children;

	SignalEvent sigchld_event;

	/**
	 * Shall the #sigchld_event be disabled automatically when there
	 * is no registered child process?  This mode should be enabled
	 * during shutdown.
	 */
	bool volatile_event = false;

public:
	ChildProcessRegistry(EventLoop &loop) noexcept;

	EventLoop &GetEventLoop() const noexcept {
		return event_loop;
	}

	void Disable() noexcept {
		sigchld_event.Disable();
	}

	bool IsEmpty() const noexcept {
		return children.empty();
	}

	/**
	 * Forget all registered children.  Call this in the new child process
	 * after forking.
	 */
	void Clear() noexcept;

	/**
	 * @param name a symbolic name for the process to be used in log
	 * messages
	 */
	void Add(pid_t pid, const char *name, ExitListener *listener) noexcept;

	void SetExitListener(pid_t pid, ExitListener *listener) noexcept;

	/**
	 * Send a signal to a child process and unregister it.
	 */
	void Kill(pid_t pid, int signo) noexcept;

	/**
	 * Send a SIGTERM to a child process and unregister it.
	 */
	void Kill(pid_t pid) noexcept;

	/**
	 * Begin shutdown of this subsystem: wait for all children to exit,
	 * and then remove the event.
	 */
	void SetVolatile() noexcept {
		volatile_event = true;
		CheckVolatileEvent();
	}

	/**
	 * Returns the number of registered child processes.
	 */
	gcc_pure
	unsigned GetCount() const noexcept {
		return children.size();
	}

private:
	gcc_pure
	ChildProcessSet::iterator FindByPid(pid_t pid) noexcept {
		return children.find(pid, ChildProcess::Compare());
	}

	void Remove(ChildProcessSet::iterator i) noexcept {
		assert(!children.empty());

		i->Disable();

		children.erase(i);
	}

	void CheckVolatileEvent() noexcept {
		if (volatile_event && IsEmpty())
			sigchld_event.Disable();
	}

	void OnExit(pid_t pid, int status, const struct rusage &rusage) noexcept;
	void OnSigChld(int signo) noexcept;
};
