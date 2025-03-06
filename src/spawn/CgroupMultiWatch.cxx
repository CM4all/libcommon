// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupMultiWatch.hxx"
#include "CgroupWatchPtr.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/Loop.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/linux/CgroupEvents.hxx"
#include "io/linux/ProcPath.hxx"
#include "util/Cast.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/PrintException.hxx"
#include "util/SharedLease.hxx"
#include "util/StringSplit.hxx"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h> // for ino_t

struct CgroupMultiWatch::Item final : IntrusiveHashSetHook<>, SharedAnchor {
	static constexpr Event::Duration BLOCK_DURATION = std::chrono::minutes{1};
	static constexpr Event::Duration EXPIRE_DURATION = std::chrono::minutes{1};

	CgroupMultiWatch &parent;

	CoarseTimerEvent expire_timer;

	const std::string name;
	const std::size_t name_hash;

	std::chrono::steady_clock::time_point blocked_until;

	class MemoryEventsWatch final : InotifyWatch {
		UniqueFileDescriptor fd;

		uint_least32_t last_oom_kill = 0;

	public:
		using InotifyWatch::InotifyWatch;

		void Open(FileDescriptor cgroup_fd) noexcept;

	private:
		/**
		 * @return true if a limit was exceeded
		 */
		bool Load() noexcept;

		// virtual methods from InotifyWatch
		void OnInotify(unsigned mask, const char *name) noexcept override;
	} memory_events_watch;

	class PidsEventsWatch final : InotifyWatch {
		UniqueFileDescriptor fd;

		uint_least32_t last_max = 0;

	public:
		using InotifyWatch::InotifyWatch;

		void Open(FileDescriptor cgroup_fd) noexcept;

	private:
		/**
		 * @return true if a limit was exceeded
		 */
		bool Load() noexcept;

		// virtual methods from InotifyWatch
		void OnInotify(unsigned mask, const char *name) noexcept override;
	} pids_events_watch;

	ino_t cgroup_id = -1;

	Item(CgroupMultiWatch &_parent, StringWithHash _name) noexcept;
	~Item() noexcept;

	auto &GetEventLoop() const noexcept {
		return expire_timer.GetEventLoop();
	}

	void BeginShutdown() noexcept {
		expire_timer.Cancel();
	}

	[[gnu::pure]]
	bool IsBlocked() const noexcept;

	void Block() noexcept;

	void SetCgroupFd(FileDescriptor cgroup_fd) noexcept;

protected:
	// virtual methods from SharedAnchor
	void OnAbandoned() noexcept override;

private:
	void OnExpireTimer() noexcept;
};

inline
CgroupMultiWatch::Item::Item(CgroupMultiWatch &_parent, StringWithHash _name) noexcept
	:parent(_parent),
	 expire_timer(parent.GetEventLoop(), BIND_THIS_METHOD(OnExpireTimer)),
	 name(_name.value), name_hash(_name.hash),
	 memory_events_watch(parent.inotify_manager),
	 pids_events_watch(parent.inotify_manager)
{
}

inline
CgroupMultiWatch::Item::~Item() noexcept = default;

inline bool
CgroupMultiWatch::Item::IsBlocked() const noexcept
{
	return parent.IsShuttingDown() ||
		GetEventLoop().SteadyNow() < blocked_until;
}

inline void
CgroupMultiWatch::Item::Block() noexcept
{
	blocked_until = GetEventLoop().SteadyNow() + BLOCK_DURATION;
}

void
CgroupMultiWatch::Item::MemoryEventsWatch::Open(FileDescriptor cgroup_fd) noexcept
{
	auto &item = ContainerCast(*this, &Item::memory_events_watch);

	RemoveWatch();

	last_oom_kill = 0;

	fd.Close();
	if (!fd.OpenReadOnly(cgroup_fd, "memory.events"))
		return;

	TryAddWatch(ProcFdPath(fd), IN_MODIFY);

	if (Load())
		item.Block();
}

bool
CgroupMultiWatch::Item::MemoryEventsWatch::Load() noexcept
try {
	assert(fd.IsDefined());

	const auto memory_events = ReadCgroupMemoryEvents(fd);
	const bool exceeded = memory_events.oom_kill > last_oom_kill;
	last_oom_kill = memory_events.oom_kill;
	return exceeded;
} catch (...) {
	PrintException(std::current_exception());
	return false;
}

void
CgroupMultiWatch::Item::MemoryEventsWatch::OnInotify([[maybe_unused]] unsigned mask,
						     [[maybe_unused]] const char *_name) noexcept
{
	auto &item = ContainerCast(*this, &Item::memory_events_watch);

	if (Load())
		item.Block();
}

void
CgroupMultiWatch::Item::PidsEventsWatch::Open(FileDescriptor cgroup_fd) noexcept
{
	auto &item = ContainerCast(*this, &Item::pids_events_watch);

	RemoveWatch();

	last_max = 0;

	fd.Close();
	if (!fd.OpenReadOnly(cgroup_fd, "pids.events"))
		return;

	TryAddWatch(ProcFdPath(fd), IN_MODIFY);

	if (Load())
		item.Block();
}

bool
CgroupMultiWatch::Item::PidsEventsWatch::Load() noexcept
try {
	assert(fd.IsDefined());

	const auto pids_events = ReadCgroupPidsEvents(fd);
	const bool exceeded = pids_events.max > last_max;
	last_max = pids_events.max;
	return exceeded;
} catch (...) {
	PrintException(std::current_exception());
	return false;
}

void
CgroupMultiWatch::Item::PidsEventsWatch::OnInotify([[maybe_unused]] unsigned mask,
						     [[maybe_unused]] const char *_name) noexcept
{
	auto &item = ContainerCast(*this, &Item::pids_events_watch);

	if (Load())
		item.Block();
}

inline void
CgroupMultiWatch::Item::SetCgroupFd(FileDescriptor cgroup_fd) noexcept
{
	assert(cgroup_fd.IsDefined());

	if (parent.IsShuttingDown())
		return;

	/* compare the cgroup ids (= inode number) to see if this is
	   still the same cgroup; it might have been deleted and
	   recreated since the last call */
	struct statx stx;
	if (statx(cgroup_fd.Get(), "", AT_EMPTY_PATH, STATX_INO, &stx) < 0)
		return;

	if (stx.stx_ino == cgroup_id)
		// no change, still the same cgroup
		return;

	blocked_until = {};
	cgroup_id = stx.stx_ino;

	memory_events_watch.Open(cgroup_fd);
	pids_events_watch.Open(cgroup_fd);
}

void
CgroupMultiWatch::Item::OnAbandoned() noexcept
{
	if (!parent.IsShuttingDown())
		expire_timer.Schedule(EXPIRE_DURATION);
}

inline void
CgroupMultiWatch::Item::OnExpireTimer() noexcept
{
	if (!IsAbandoned())
		return;

	parent.items.erase_and_dispose(parent.items.iterator_to(*this),
				       DeleteDisposer{});
}

constexpr StringWithHash
CgroupMultiWatch::GetName::operator()(const Item &item) const noexcept
{
	return StringWithHash{item.name, item.name_hash};
}

CgroupMultiWatch::CgroupMultiWatch(EventLoop &event_loop)
	:inotify_manager(event_loop)
{
}

CgroupMultiWatch::~CgroupMultiWatch() noexcept
{
	items.clear_and_dispose(DeleteDisposer{});
}

void
CgroupMultiWatch::BeginShutdown() noexcept
{
	inotify_manager.BeginShutdown();

	items.for_each([](auto &i){
		i.BeginShutdown();
	});
}

CgroupWatchPtr
CgroupMultiWatch::Get(StringWithHash name) noexcept
{
	auto [it, inserted] = items.insert_check(name);
	if (inserted) {
		auto *item = new Item(*this, name);
		it = items.insert_commit(it, *item);
	}

	return *it;
}

bool
CgroupWatchPtr::IsBlocked() const noexcept
{
	if (!lease)
		return false;

	const auto &item = static_cast<const CgroupMultiWatch::Item &>(lease.GetAnchor());
	return item.IsBlocked();
}

void
CgroupWatchPtr::SetCgroup(FileDescriptor cgroup_fd) noexcept
{
	if (!lease)
		return;

	auto &item = static_cast<CgroupMultiWatch::Item &>(lease.GetAnchor());
	item.SetCgroupFd(cgroup_fd);
}
