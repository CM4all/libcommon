/*
 * Copyright 2016-2022 Max Kellermann <max.kellermann@gmail.com>
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

#include "LeakDetector.hxx"

#ifndef NDEBUG

#include <cassert>
#include <mutex>

class LeakDetectorContainer {
	std::mutex mutex;

	IntrusiveList<LeakDetector, IntrusiveListMemberHookTraits<&LeakDetector::leak_detector_siblings>> list;

public:
	~LeakDetectorContainer() noexcept {
		assert(list.empty());
	}

	void Add(LeakDetector &l) noexcept {
		const std::scoped_lock lock{mutex};
		list.push_back(l);
	}

	void Remove(LeakDetector &l) noexcept {
		const std::scoped_lock lock{mutex};
		list.erase(list.iterator_to(l));
	}
};

static LeakDetectorContainer leak_detector_container;

LeakDetector::LeakDetector() noexcept
{
	leak_detector_container.Add(*this);
	state = State::REGISTERED;
}

LeakDetector::~LeakDetector() noexcept
{
	assert(state == State::REGISTERED);

	leak_detector_container.Remove(*this);
	state = State::DESTRUCTED;
}

void
LeakDetector::Dummy() noexcept
{
}

#endif
