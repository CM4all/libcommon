// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Cast.hxx"
#include "Concepts.hxx"
#include "IntrusiveHookMode.hxx"
#include "MemberPointer.hxx"
#include "OptionalCounter.hxx"

#include <algorithm> // for std::all_of()
#include <array>
#include <bit> // for std::rotr()
#include <cassert>
#include <numeric> // for std::accumulate()

struct IntrusiveHashArrayTrieOptions {
	bool constant_time_size = false;
};

struct IntrusiveHashArrayTrieItem;

struct IntrusiveHashArrayTrieNode {
	static constexpr unsigned INDEX_BITS = 2;
	static constexpr std::size_t ARRAY_SIZE = 1 << INDEX_BITS;
	static constexpr std::size_t INDEX_MASK = ARRAY_SIZE - 1;

	std::array<IntrusiveHashArrayTrieItem *, ARRAY_SIZE> children;

	static constexpr std::size_t GetIndexByHash(std::size_t hash) noexcept {
		return hash & INDEX_MASK;
	}

	[[gnu::pure]]
	constexpr std::size_t size() const noexcept;

	[[nodiscard]]
	constexpr IntrusiveHashArrayTrieItem *FindNextChild(std::size_t i) const noexcept {
		for (; i < children.size(); ++i)
			if (children[i] != nullptr)
				return children[i];

		return nullptr;
	}

	/**
	 * @param item a single item (assumed to have no children)
	 * whose #rotated_hash field has been calculated and rotated
	 * to be inserted right as this node's child
	 */
	constexpr void insert(IntrusiveHashArrayTrieItem &item) noexcept;

	constexpr void SwapChildren(IntrusiveHashArrayTrieNode &other) noexcept;
};

struct IntrusiveHashArrayTrieItem : IntrusiveHashArrayTrieNode {
	IntrusiveHashArrayTrieNode *parent;

	std::size_t rotated_hash;

	constexpr void Reparent([[maybe_unused]] IntrusiveHashArrayTrieNode &old_parent,
				IntrusiveHashArrayTrieNode &new_parent) noexcept {
		assert(parent == &old_parent);
		parent = &new_parent;
	}

	constexpr std::size_t GetIndexInParent() const noexcept {
		return GetIndexByHash(rotated_hash);
	}

	[[gnu::pure]]
	constexpr std::size_t size() const noexcept {
		return 1 + IntrusiveHashArrayTrieNode::size();
	}

	/**
	 * @return the child that replaced this item in its parent (or
	 * nullptr if the item had no children)
	 */
	constexpr IntrusiveHashArrayTrieItem *unlink() noexcept {
		assert(parent != nullptr);

		auto &parent_slot = parent->children[GetIndexInParent()];
		assert(parent_slot == this);

		/* choose a child that gets moved one up in the trie,
		   replacing us as the container for the other
		   children */
		std::size_t chosen_index = 0;
		while (children[chosen_index] == nullptr) {
			if (++chosen_index >= children.size()) {
				/* no children - simply clear the parent slot */
				parent->children[GetIndexInParent()] = nullptr;
				return nullptr;
			}
		}

		/* found a child - rotate its hash back for the new
		   trie position and replace it in our parent */

		auto &chosen_child = *children[chosen_index];
		children[chosen_index] = nullptr;

		assert(chosen_child.parent == this);
		assert(chosen_child.GetIndexInParent() == chosen_index);

		chosen_child.rotated_hash = std::rotl(chosen_child.rotated_hash, INDEX_BITS);
		assert(chosen_child.GetIndexInParent() == GetIndexInParent());

		parent_slot = &chosen_child;
		chosen_child.parent = parent;

		/* move all remaining children over; we swap because
		   we need chosen_child's previous children list for
		   the next step */

		SwapChildren(chosen_child);

		/* now reinsert chosen_children's previous children
		   list */

		for (IntrusiveHashArrayTrieItem *i : children) {
			if (i != nullptr) {
				assert(children[i->GetIndexInParent()] == i);

				chosen_child.insert(*i);
			}
		}

		return &chosen_child;
	}

	constexpr bool is_linked() const noexcept {
		return parent != nullptr;
	}
};

constexpr std::size_t
IntrusiveHashArrayTrieNode::size() const noexcept
{
	std::size_t n = 0;
	for (const IntrusiveHashArrayTrieItem *i : children) {
		if (i != nullptr)
			n += i->size();
	}

	return n;
}

constexpr void
IntrusiveHashArrayTrieNode::insert(IntrusiveHashArrayTrieItem &item) noexcept
{
	IntrusiveHashArrayTrieNode *p = this;

	while (true) {
		auto &slot = p->children[item.GetIndexInParent()];
		if (slot == nullptr) {
			slot = &item;
			item.children = {};
			item.parent = p;
			return;
		}

		p = slot;
		item.rotated_hash = std::rotr(item.rotated_hash, INDEX_BITS);
	}
}

constexpr void
IntrusiveHashArrayTrieNode::SwapChildren(IntrusiveHashArrayTrieNode &other) noexcept
{
	using std::swap;

	for (std::size_t i = 0; i < ARRAY_SIZE; ++i) {
		swap(children[i], other.children[i]);

		if (children[i] != nullptr)
			children[i]->Reparent(other, *this);

		if (other.children[i] != nullptr)
			other.children[i]->Reparent(*this, other);
	}
}

/**
 * @param Tag an arbitrary tag type to allow using multiple base hooks
 */
template<IntrusiveHookMode _mode=IntrusiveHookMode::NORMAL,
	 typename Tag=void>
class IntrusiveHashArrayTrieHook {
	template<typename, typename> friend struct IntrusiveHashArrayTrieBaseHookTraits;
	template<auto> friend struct IntrusiveHashArrayTrieMemberHookTraits;
	template<typename T, typename, typename, IntrusiveHashArrayTrieOptions> friend class IntrusiveHashArrayTrie;

	static constexpr IntrusiveHookMode mode = _mode;

	IntrusiveHashArrayTrieItem item;

public:
	constexpr IntrusiveHashArrayTrieHook() noexcept {
		if constexpr (mode >= IntrusiveHookMode::TRACK)
			item.parent = nullptr;
	}

	constexpr ~IntrusiveHashArrayTrieHook() noexcept {
		if constexpr (mode >= IntrusiveHookMode::AUTO_UNLINK)
			if (is_linked())
				unlink();
	}

	constexpr void unlink() noexcept {
		if constexpr (mode >= IntrusiveHookMode::TRACK) {
			assert(is_linked());
		}

		item.unlink();

		if constexpr (mode >= IntrusiveHookMode::TRACK)
			item.parent = nullptr;
	}

	constexpr bool is_linked() const noexcept
		requires(mode >= IntrusiveHookMode::TRACK) {
		return item.is_linked();
	}

private:
	static constexpr auto &Cast(IntrusiveHashArrayTrieItem &item) noexcept {
		return ContainerCast(item, &IntrusiveHashArrayTrieHook::item);
	}

	static constexpr const auto &Cast(const IntrusiveHashArrayTrieItem &item) noexcept {
		return ContainerCast(item, &IntrusiveHashArrayTrieHook::item);
	}
};

/**
 * For classes which embed #IntrusiveHashArrayTrieHook as base class.
 *
 * @param Tag selector for which #IntrusiveHashArrayTrieHook to use
 */
template<typename T, typename Tag=void>
struct IntrusiveHashArrayTrieBaseHookTraits {
	/* a never-called helper function which is used by _Cast() */
	template<IntrusiveHookMode mode>
	static constexpr IntrusiveHashArrayTrieHook<mode, Tag> _Identity(const IntrusiveHashArrayTrieHook<mode, Tag> &) noexcept;

	/* another never-called helper function which "calls"
	   _Identity(), implicitly casting the item to the
	   IntrusiveHashArrayTrieHook specialization; we use this to detect
	   which IntrusiveHashArrayTrieHook specialization is used */
	template<typename U>
	static constexpr auto _Cast(const U &u) noexcept {
		return decltype(_Identity(u))();
	}

	template<typename U>
	using Hook = decltype(_Cast(std::declval<U>()));

	static constexpr T *Cast(IntrusiveHashArrayTrieItem *item) noexcept {
		auto *hook = &Hook<T>::Cast(*item);
		return static_cast<T *>(hook);
	}

	static constexpr auto &ToHook(T &t) noexcept {
		return static_cast<Hook<T> &>(t);
	}
};

/**
 * For classes which embed #IntrusiveListHook as member.
 */
template<auto member>
struct IntrusiveHashArrayTrieMemberHookTraits {
	using T = MemberPointerContainerType<decltype(member)>;
	using _Hook = MemberPointerType<decltype(member)>;

	template<typename Dummy>
	using Hook = _Hook;

	static constexpr T *Cast(Hook<T> *node) noexcept {
		return &ContainerCast(*node, member);
	}

	static constexpr auto &ToHook(T &t) noexcept {
		return t.*member;
	}
};

/**
 * @param GetKey a function object which extracts the "key" part of an
 * item
 */
template<typename Hash, typename Equal,
	 typename GetKey=std::identity>
struct IntrusiveHashArrayTrieOperators {
	using hasher = Hash;
	using key_equal = Equal;

	[[no_unique_address]]
	Hash hash;

	[[no_unique_address]]
	Equal equal;

	[[no_unique_address]]
	GetKey get_key;
};

/**
 * A hash array mapped trie (HAMT) implementation which stores
 * pointers to items which have an embedded
 * #IntrusiveHashArrayTrieHook.
 *
 * @param Operators a class which contains functions `hash` and
 * `equal`
 */
template<typename T,
	 typename Operators,
	 typename HookTraits=IntrusiveHashArrayTrieBaseHookTraits<T>,
	 IntrusiveHashArrayTrieOptions options=IntrusiveHashArrayTrieOptions{}>
class IntrusiveHashArrayTrie {
	static constexpr bool constant_time_size = options.constant_time_size;

	[[no_unique_address]]
	OptionalCounter<constant_time_size> counter;

	[[no_unique_address]]
	Operators ops;

	IntrusiveHashArrayTrieNode root{};

	static constexpr auto GetHookMode() noexcept {
		return HookTraits::template Hook<T>::mode;
	}

	static constexpr T *Cast(IntrusiveHashArrayTrieItem *item) noexcept {
		return HookTraits::Cast(item);
	}

	static constexpr const T *Cast(const IntrusiveHashArrayTrieItem *item) noexcept {
		return HookTraits::Cast(const_cast<IntrusiveHashArrayTrieItem *>(item));
	}

	static constexpr auto &ToHook(T &t) noexcept {
		return HookTraits::ToHook(t);
	}

	static constexpr const auto &ToHook(const T &t) noexcept {
		return HookTraits::ToHook(const_cast<T &>(t));
	}

	static constexpr IntrusiveHashArrayTrieNode &ToNode(T &t) noexcept {
		return ToHook(t).siblings;
	}

	static constexpr const IntrusiveHashArrayTrieNode &ToNode(const T &t) noexcept {
		return ToHook(t).siblings;
	}

public:
	using value_type = T;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using size_type = std::size_t;

	using hasher = typename Operators::hasher;
	using key_equal = typename Operators::key_equal;

	[[nodiscard]]
	IntrusiveHashArrayTrie() noexcept = default;

	constexpr IntrusiveHashArrayTrie(IntrusiveHashArrayTrie &&src) noexcept {
		std::copy(src.root.children.begin(), src.root.children.end(),
			  root.children.begin());

		for (IntrusiveHashArrayTrieItem *item : root.children) {
			if (item != nullptr) {
				item->Reparent(src.root, root);
			}
		}

		src.root.children = {};

		using std::swap;
		swap(counter, src.counter);
	}

	constexpr ~IntrusiveHashArrayTrie() noexcept {
		if constexpr (GetHookMode() >= IntrusiveHookMode::TRACK)
			clear();
	}

	IntrusiveHashArrayTrie &operator=(IntrusiveHashArrayTrie &&src) noexcept = delete;

	[[nodiscard]]
	constexpr const hasher &hash_function() const noexcept {
		return ops.hash;
	}

	[[nodiscard]]
	constexpr const key_equal &key_eq() const noexcept {
		return ops.equal;
	}

	[[nodiscard]]
	constexpr bool empty() const noexcept {
		if constexpr (constant_time_size)
			return size() == 0;
		else
			return std::all_of(root.children.begin(), root.children.end(), [](const auto &slot){
				return slot == nullptr;
			});
	}

	[[nodiscard]]
	constexpr size_type size() const noexcept {
		if constexpr (constant_time_size)
			return counter;
		else
			return root.size();
	}

	constexpr void clear() noexcept {
		root.children = {};
		counter.reset();
	}

	constexpr void swap(IntrusiveHashArrayTrie &other) noexcept {
		using std::swap;

		root.SwapChildren(other.root);

		swap(counter, other.counter);
	}

private:
	static constexpr void DisposeChildren(IntrusiveHashArrayTrieNode &node,
					      Disposer<value_type> auto disposer) noexcept {
		for (IntrusiveHashArrayTrieItem *i : node.children) {
			if (i == nullptr)
				continue;

			assert(node.children[i->GetIndexInParent()] == i);

			DisposeChildren(*i, disposer);
			disposer(Cast(i));
		}
	}

	[[nodiscard]]
	static constexpr IntrusiveHashArrayTrieItem *NextItem(const IntrusiveHashArrayTrieNode &root,
							      IntrusiveHashArrayTrieItem *i) noexcept {
		assert(i != nullptr);

		if (auto *child = i->FindNextChild(0))
			return child;

		while (true) {
			auto *parent = i->parent;
			assert(parent != nullptr);

			const std::size_t idx = i->GetIndexInParent();
			assert(parent->children[idx] == i);

			if (auto *next = parent->FindNextChild(idx + 1))
				return next;

			if (parent == &root)
				/* we're already at the root, can't
				   backtrace more, there are no more
				   items */
				return nullptr;

			/* this isn't the root, so this is really an
			   IntrusiveHashArrayTrieItem - try again
			   here */
			i = reinterpret_cast<IntrusiveHashArrayTrieItem *>(parent);
		}
	}

public:
	constexpr void clear_and_dispose(Disposer<value_type> auto disposer) noexcept {
		DisposeChildren(root, disposer);
		root.children = {};

		counter.reset();
	}

	class const_iterator;

	class iterator final {
		friend IntrusiveHashArrayTrie;
		friend const_iterator;

		IntrusiveHashArrayTrieNode *root;
		IntrusiveHashArrayTrieItem *cursor;

		[[nodiscard]]
		constexpr iterator(IntrusiveHashArrayTrieNode *_root,
				   IntrusiveHashArrayTrieItem *_cursor) noexcept
			:root(_root), cursor(_cursor) {}

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using reference = value_type &;

		[[nodiscard]]
		constexpr iterator() noexcept = default;

		constexpr bool operator==(const iterator &other) const noexcept {
			return cursor == other.cursor;
		}

		constexpr bool operator!=(const iterator &other) const noexcept {
			return cursor != other.cursor;
		}

		constexpr reference operator*() const noexcept {
			return *Cast(cursor);
		}

		constexpr pointer operator->() const noexcept {
			return Cast(cursor);
		}

		constexpr auto &operator++() noexcept {
			cursor = NextItem(*root, cursor);
			return *this;
		}

		constexpr auto operator++(int) noexcept {
			auto old = *this;
			cursor = NextItem(*root, cursor);
			return old;
		}

	};

	constexpr iterator begin() noexcept {
		for (IntrusiveHashArrayTrieItem *i : root.children)
			if (i != nullptr)
				return {&root, i};

		return {&root, nullptr};
	}

	constexpr iterator end() noexcept {
		return {&root, nullptr};
	}

	[[nodiscard]]
	constexpr iterator iterator_to(reference item) noexcept {
		return {&root, &ToHook(item).item};
	}

	class const_iterator final {
		friend IntrusiveHashArrayTrie;

		const IntrusiveHashArrayTrieNode *root;
		const IntrusiveHashArrayTrieItem *cursor;

		[[nodiscard]]
		constexpr const_iterator(const IntrusiveHashArrayTrieNode *_root,
					 const IntrusiveHashArrayTrieItem *_cursor) noexcept
			:root(_root), cursor(_cursor) {}

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = const T;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using reference = value_type &;

		[[nodiscard]]
		constexpr const_iterator() noexcept = default;

		[[nodiscard]]
		constexpr const_iterator(iterator src) noexcept
			:root(src.root), cursor(src.cursor) {}

		constexpr bool operator==(const const_iterator &other) const noexcept {
			return cursor == other.cursor;
		}

		constexpr bool operator!=(const const_iterator &other) const noexcept {
			return cursor != other.cursor;
		}

		constexpr reference operator*() const noexcept {
			return *Cast(cursor);
		}

		constexpr pointer operator->() const noexcept {
			return Cast(cursor);
		}

		constexpr auto &operator++() noexcept {
			cursor = NextItem(*root,
					  const_cast<IntrusiveHashArrayTrieItem *>(cursor));
			return *this;
		}

		constexpr auto operator++(int) noexcept {
			auto old = *this;
			cursor = NextItem(*root,
					  const_cast<IntrusiveHashArrayTrieItem *>(cursor));
			return old;
		}

	};

	[[nodiscard]]
	constexpr const_iterator begin() const noexcept {
		for (IntrusiveHashArrayTrieItem *i : root.children)
			if (i != nullptr)
				return {&root, i};

		return {&root, nullptr};
	}

	[[nodiscard]]
	constexpr const_iterator end() const noexcept {
		return {&root, nullptr};
	}

	[[nodiscard]]
	constexpr const_iterator iterator_to(const_reference item) const noexcept {
		return {&root, &ToHook(item).item};
	}

	/**
	 * Insert a new item without checking whether the key already
	 * exists.
	 */
	constexpr iterator insert(reference item) noexcept {
		++counter;
		auto &node = ToHook(item).item;
		node.rotated_hash = ops.hash(ops.get_key(item));
		root.insert(node);
		return {&root, &node};
	}

	constexpr void erase(iterator i) noexcept {
		assert(i != end());

		--counter;
		i.cursor->unlink();

		if constexpr (GetHookMode() >= IntrusiveHookMode::TRACK)
			i.cursor->parent = nullptr;
	}

	constexpr void erase_and_dispose(iterator i,
					 Disposer<value_type> auto disposer) noexcept {
		erase(i);
		disposer(&*i);
	}

	/**
	 * Remove and dispose all items with the specified key.
	 *
	 * @return the number of removed items
	 */
	constexpr std::size_t remove_and_dispose_key(const auto &key,
						     Disposer<value_type> auto disposer) noexcept {
		std::size_t hash = ops.hash(key);

		IntrusiveHashArrayTrieNode *node = &root;

		std::size_t n_removed = 0;
		while (true) {
			const std::size_t idx = IntrusiveHashArrayTrieNode::GetIndexByHash(hash);

			IntrusiveHashArrayTrieItem *item = node->children[idx];
			if (item == nullptr)
				return n_removed;

			assert(item->GetIndexInParent() == idx);

			while (item->rotated_hash == hash &&
			       ops.equal(key, ops.get_key(*Cast(item)))) {
				auto *replacement = item->unlink();
				disposer(Cast(item));
				++n_removed;

				if (replacement == nullptr)
					return n_removed;

				item = replacement;
			}

			/* go one level deeper, rotate the hash */
			node = item;
			hash = std::rotr(hash, IntrusiveHashArrayTrieNode::INDEX_BITS);
		}
	}

	/**
	 * Like find(), but returns an item that matches the given
	 * predicate.  This is useful if the container can contain
	 * multiple items that compare equal (according to #Equal, but
	 * not according to #pred).
	 */
	[[nodiscard]] [[gnu::pure]]
	constexpr iterator find_if(const auto &key,
				   std::predicate<const_reference> auto pred) noexcept {
		std::size_t hash = ops.hash(key);

#ifndef NDEBUG
		std::size_t inverse_hash_mask = ~std::size_t{};
#endif

		IntrusiveHashArrayTrieNode *node = &root;

		while (true) {
			const std::size_t idx = IntrusiveHashArrayTrieNode::GetIndexByHash(hash);

			IntrusiveHashArrayTrieItem *item = node->children[idx];
			if (item == nullptr)
				return end();

			assert(item->parent == node);
			assert(item->GetIndexInParent() == idx);

#ifndef NDEBUG
			inverse_hash_mask >>= IntrusiveHashArrayTrieNode::INDEX_BITS;
			const std::size_t hash_mask = ~std::rotl(inverse_hash_mask, IntrusiveHashArrayTrieNode::INDEX_BITS);
#endif
			assert((item->rotated_hash & hash_mask) == (hash & hash_mask));

			if (item->rotated_hash == hash &&
			    ops.equal(key, ops.get_key(*Cast(item))) &&
			    pred(*Cast(item)))
				return {&root, item};

			node = item;
			hash = std::rotr(hash, IntrusiveHashArrayTrieNode::INDEX_BITS);
		}
	}

	[[nodiscard]] [[gnu::pure]]
	constexpr iterator find(const auto &key) noexcept {
		return find_if(key, [](const auto &){ return true; });
	}

	[[nodiscard]] [[gnu::pure]]
	constexpr const_iterator find(const auto &key) const noexcept {
		return const_cast<IntrusiveHashArrayTrie *>(this)->find(key);
	}

	class equal_iterator final {
		friend IntrusiveHashArrayTrie;

		const_iterator it;
		const IntrusiveHashArrayTrieItem *initial;

		[[no_unique_address]]
		Operators ops;

		[[nodiscard]]
		constexpr equal_iterator(const_iterator _it) noexcept
			:it(_it), initial(_it.cursor) {}

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = const T;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using reference = value_type &;

		constexpr bool operator==(const equal_iterator &other) const noexcept {
			return it == other.it;
		}

		constexpr bool operator!=(const equal_iterator &other) const noexcept {
			return it != other.it;
		}

		constexpr bool operator==(const const_iterator &other) const noexcept {
			return it == other;
		}

		constexpr bool operator!=(const const_iterator &other) const noexcept {
			return it != other;
		}

		constexpr reference operator*() const noexcept {
			return *it;
		}

		constexpr pointer operator->() const noexcept {
			return it.operator->();
		}

		constexpr auto &operator++() noexcept {
			do {
				++it;
			} while (it.cursor != nullptr &&
				 !ops.equal(ops.get_key(*Cast(initial)),
					    ops.get_key(*it)));

			return *this;
		}

		constexpr auto operator++(int) noexcept {
			auto old = *this;
			operator++();
			return old;
		}
	};

	constexpr std::pair<equal_iterator, equal_iterator> equal_range(const auto &key) const noexcept {
		return {find(key), end()};
	}

	constexpr void for_each(const auto &key,
				std::invocable<const_reference> auto f) const {
		std::size_t hash = ops.hash(key);

		const IntrusiveHashArrayTrieNode *node = &root;

		while (true) {
			const std::size_t idx = IntrusiveHashArrayTrieNode::GetIndexByHash(hash);

			const IntrusiveHashArrayTrieItem *item = node->children[idx];
			if (item == nullptr)
				break;

			assert(item->GetIndexInParent() == idx);

			if (item->rotated_hash == hash &&
			    ops.equal(key, ops.get_key(*Cast(item))))
				f(*Cast(item));

			node = item;
			hash = std::rotr(hash, IntrusiveHashArrayTrieNode::INDEX_BITS);
		}
	}
};
