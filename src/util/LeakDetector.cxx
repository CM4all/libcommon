// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
