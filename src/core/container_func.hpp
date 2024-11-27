/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file container_func.hpp Some simple functions to help with accessing containers. */

#ifndef CONTAINER_FUNC_HPP
#define CONTAINER_FUNC_HPP

/**
 * Helper function to append an item to a container if it is not already contained.
 * The container must have a \c emplace_back function.
 * Consider using std::set, std::unordered_set or std::flat_set in new code.
 *
 * @param container A reference to the container to be extended
 * @param item Reference to the item to be copy-constructed if not found
 *
 * @return Whether the item was already present
 */
template <typename Container>
inline bool include(Container &container, typename Container::const_reference &item)
{
	const bool is_member = std::ranges::find(container, item) != container.end();
	if (!is_member) container.emplace_back(item);
	return is_member;
}

/**
 * Helper function to get the index of an item
 * Consider using std::set, std::unordered_set or std::flat_set in new code.
 *
 * @param container A reference to the container to be searched.
 * @param item Reference to the item to be search for
 *
 * @return Index of element if found, otherwise -1
 */
template <typename Container>
int find_index(Container const &container, typename Container::const_reference item)
{
	auto const it = std::ranges::find(container, item);
	if (it != container.end()) return std::distance(container.begin(), it);

	return -1;
}

/**
 * Move elements between first and last to a new position, rotating elements in between as necessary.
 * @param first Iterator to first element to move.
 * @param last Iterator to (end-of) last element to move.
 * @param position Iterator to where range should be moved to.
 * @returns Iterators to first and last after being moved.
 */
template <typename TIter>
auto Slide(TIter first, TIter last, TIter position) -> std::pair<TIter, TIter>
{
	if (last < position) return { std::rotate(first, last, position), position };
	if (position < first) return { position, std::rotate(position, first, last) };
	return { first, last };
}

#endif /* CONTAINER_FUNC_HPP */
