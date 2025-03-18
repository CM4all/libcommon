// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "TmpfsManager.hxx"
#include "TmpfsCreate.hxx"
#include "system/Error.hxx"
#include "system/Mount.hxx"
#include "util/djb_hash.hxx"
#include "util/SharedLease.hxx"
#include "util/SpanCast.hxx"

#include <cassert>
#include <chrono>
#include <string>

#include <fcntl.h> // AT_REMOVEDIR
#include <sys/stat.h> // for mkdir()

static void
MkdirMount(FileDescriptor from_fd,
	   FileDescriptor to_fd, const char *to_path)
{
	if (mkdirat(to_fd.Get(), to_path, 0100) < 0)
		throw MakeErrno("Failed to create tmpfs mountpoint");

	MoveMount(from_fd, "", to_fd, to_path,
		  MOVE_MOUNT_F_EMPTY_PATH);
}

static void
UmountRmdir(FileDescriptor fd, const char *path)
{
	// TODO hard-coded path
	constexpr const char *tmp = "/var/tmp";

	UmountDetachAt(fd, path, 0, tmp);

	if (unlinkat(fd.Get(), path, AT_REMOVEDIR) < 0)
		throw MakeErrno("Failed to delete tmpfs mountpoint");
}

struct TmpfsManager::Item final
	: IntrusiveHashSetHook<>, IntrusiveListHook<>, SharedAnchor
{
	TmpfsManager &manager;

	std::chrono::steady_clock::time_point expires;

	std::string name;

	UniqueFileDescriptor fd;

	Item(TmpfsManager &_manager,
	     std::string_view _name, UniqueFileDescriptor &&_fd) noexcept
		:manager(_manager),
		 name(_name),
		 fd(std::move(_fd))
	{
		/* we need to keep the new tmpfs mounted somewhere or
		   else open_tree() always returns EINVAL */
		MkdirMount(fd, manager.mnt, name.c_str());
	}

	~Item() noexcept {
		/* unmount the tmpfs and delete it */
		UmountRmdir(manager.mnt, name.c_str());
	}

	Item(const Item &) = delete;
	Item &operator=(const Item &) = delete;

	/**
	 * Create a clone of the tmpfs mount which can then be passed
	 * to move_mount() into a new mount namespace.
	 */
	UniqueFileDescriptor Clone() const {
		return OpenTree(fd, "", AT_EMPTY_PATH|OPEN_TREE_CLONE);
	}

	// virtual methods from SharedAnchor
	void OnAbandoned() noexcept override {
		// TODO hard-coded expiry
		expires = std::chrono::steady_clock::now() + std::chrono::hours{1};
		manager.abandoned.push_back(*this);
	}

	// TDOO void OnBroken() noexcept override;
};

inline std::size_t
TmpfsManager::ItemHash::operator()(std::string_view name) const noexcept
{
	return djb_hash(AsBytes(name));
}

inline std::string_view
TmpfsManager::ItemGetKey::operator()(const Item &item) const noexcept
{
	return item.name;
}

TmpfsManager::TmpfsManager(UniqueFileDescriptor _mnt) noexcept
	:mnt(std::move(_mnt)) {}

TmpfsManager::~TmpfsManager() noexcept
{
	abandoned.clear_and_dispose([this](auto *item){
		items.erase(items.iterator_to(*item));
		delete item;
	});

	assert(items.empty());
}

void
TmpfsManager::Expire() noexcept
{
	if (abandoned.empty())
		return;

	const auto now = std::chrono::steady_clock::now();
	do {
		auto *item = &abandoned.front();
		if (item->expires > now)
			break;

		abandoned.pop_front();
		items.erase(items.iterator_to(*item));
		delete item;
	} while (!abandoned.empty());
}

TmpfsManager::Result
TmpfsManager::MakeTmpfs(std::string_view name, bool exec)
{
	auto [i, inserted] = items.insert_check(name);

	if (inserted) {
		auto *item = new Item(*this, name, CreateTmpfs(exec));
		i = items.insert_commit(i, *item);
	} else if (i->IsAbandoned()) {
		abandoned.erase(abandoned.iterator_to(*i));
	}

	return {i->Clone(), *i};
}
