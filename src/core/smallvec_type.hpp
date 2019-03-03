/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallvec_type.hpp Simple vector class that allows allocating an item without the need to copy this->data needlessly. */

#ifndef SMALLVEC_TYPE_HPP
#define SMALLVEC_TYPE_HPP

#include "alloc_func.hpp"
#include "mem_func.hpp"
#include <vector>
#include <algorithm>

/**
 * Helper function to append an item to a vector if it is not already contained
 * Consider using std::set, std::unordered_set or std::flat_set in new code
 *
 * @param vec A reference to the vector to be extended
 * @param item Reference to the item to be copy-constructed if not found
 *
 * @return Whether the item was already present
 */
template <typename T>
inline bool include(std::vector<T>& vec, const T &item)
{
	const bool is_member = std::find(vec.begin(), vec.end(), item) != vec.end();
	if (!is_member) vec.emplace_back(item);
	return is_member;
}


/**
 * Simple vector template class.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @tparam T The type of the items stored
 * @tparam S The steps of allocation
 */


template <typename T, uint S>
using SmallVector = std::vector<T>;

/**
 * Helper function to get the index of an item
 * Consider using std::set, std::unordered_set or std::flat_set in new code
 *
 * @param vec A reference to the vector to be extended
 * @param item Reference to the item to be search for
 *
 * @return Index of element if found, otherwise -1
 */
template <typename T>
int find_index(std::vector<T> const& vec, T const& item)
{
	auto const it = std::find(vec.begin(), vec.end(), item);
	if (it != vec.end()) return it - vec.begin();

	return -1;
}

/**
 * Helper function to append N default-constructed elements and get a pointer to the first new element
 * Consider using std::back_inserter in new code
 *
 * @param vec A reference to the vector to be extended
 * @param num The number of elements to default-construct
 *
 * @return Pointer to the first new element
 */
template <typename T>
inline T* grow(std::vector<T>& vec, std::size_t num)
{
	const std::size_t pos = vec.size();
	vec.resize(pos + num);
	return vec.data() + pos;
}


/**
 * Simple vector template class, with automatic free.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @param T The type of the items stored, must be a pointer
 * @param S The steps of allocation
 */
template <typename T, uint S>
class AutoFreeSmallVector : public SmallVector<T, S> {
public:
	~AutoFreeSmallVector()
	{
		this->Clear();
	}

	/**
	 * Remove all items from the list.
	 */
	inline void Clear()
	{
		for (uint i = 0; i < std::vector<T>::size(); i++) {
			free(std::vector<T>::operator[](i));
		}

		std::vector<T>::clear();
	}
};

/**
 * Simple vector template class, with automatic delete.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @param T The type of the items stored, must be a pointer
 * @param S The steps of allocation
 */
template <typename T, uint S>
class AutoDeleteSmallVector : public SmallVector<T, S> {
public:
	~AutoDeleteSmallVector()
	{
		this->Clear();
	}

	/**
	 * Remove all items from the list.
	 */
	inline void Clear()
	{
		for (uint i = 0; i < std::vector<T>::size(); i++) {
			delete std::vector<T>::operator[](i);
		}

		std::vector<T>::clear();
	}
};

typedef AutoFreeSmallVector<char*, 4> StringList; ///< Type for a list of strings.

#endif /* SMALLVEC_TYPE_HPP */
