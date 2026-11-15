/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file saveload_oldpool.h Helper class to handle conversion of objects from pool storage. */

#ifndef SAVELOAD_OLDPOOL_H
#define SAVELOAD_OLDPOOL_H

/**
 * Helper class to handle conversion of linked-list style pools to plain vectors.
 * @tparam T type of each item.
 */
template <typename T>
class OldSaveLoadPool {
public:
	/** Container for each item with linked-list indexes. */
	struct Item {
		uint32_t index = 0; ///< This item's index (1-based).
		uint32_t next = 0; ///< The next item's index (1-based).
		T value{}; ///< The item's value.
	};

	/**
	 * Clear all items from this old pool.
	 */
	void Clear()
	{
		this->items.clear();
		this->items.shrink_to_fit();
	}

	/**
	 * Get a pointer to an old item with the given reference index.
	 * @param ref_index Reference index (one-based) to get.
	 * @return Pointer to old item, or nullptr if not present.
	 */
	Item *Get(size_t ref_index)
	{
		if (ref_index == 0) return nullptr;
		assert(ref_index <= this->items.size());
		return &this->items[ref_index - 1];
	}

	/**
	 * Allocate an old item with the given pool index.
	 * @param pool_index Pool index (zero-based) to allocate.
	 * @return Reference to allocated old item.
	 */
	Item &Allocate(size_t pool_index)
	{
		assert(pool_index < UINT32_MAX);
		if (pool_index >= this->items.size()) this->items.resize(pool_index + 1);
		return this->items[pool_index];
	}

	/**
	 * Traverse an old linked-list of items and perform an action on each one in turn.
	 * @param ref_index Reference index (one-based) to start from.
	 * @param action The action to perform.
	 */
	template <typename Taction>
	void TransformItems(size_t ref_index, Taction action)
	{
		for (Item *item = this->Get(ref_index); item != nullptr; item = this->Get(item->next)) {
			action(item->value);
		}
	}

	/**
	 * Convert an old linked-list of items into a plain vector of items.
	 * @param ref_index Reference index (one-based) to start from.
	 * @return Vector of items.
	 */
	std::vector<T> TransformToVector(size_t ref_index)
	{
		std::vector<T> list;
		TransformItems(ref_index, [&list](const auto &item) { list.push_back(std::move(item)); });
		return list;
	}

	std::vector<Item> items{}; ///< Temporary storage for loading old pool items for later conversion.
};

#endif /* SAVELOAD_OLDPOOL_H */
