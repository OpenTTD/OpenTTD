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

#endif /* SMALLVEC_TYPE_HPP */
