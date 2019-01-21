/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file areaquery_type.hpp 2D segment tree class; O(1) insertion of area, O(K + R*C) retrieval of items within a given area (where K is the number of items in an area at most 4x the size of the given area, and R and C are the effective number of rows and columns being queried respectively). */

#ifndef AREAQUERY_TYPE_HPP
#define AREAQUERY_TYPE_HPP

#include "linearquery_type.hpp"

#include <memory>

/**
 * Area query template class.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @tparam T The type of the items stored
 */
template <typename T>
class AreaQueryTree {
private:
	SegmentTree<LinearQueryTree<T>> data;

public:
	AreaQueryTree() {}

	/**
	 * Constructs an empty area query tree with the given size.
	 * @param height The height of the tree (expressed as a power of two).
	 * @param width The width of the tree (expressed as a power of two).
	 */
	AreaQueryTree(uint8 height, uint8 width) : data(height)
	{
		this->data.ForEachElement([&](LinearQueryTree<T>& tree) {
			tree.Resize(width);
		});
	}

	/**
	 * Clear all the data from the tree.  Note: It can be made faster by ensuring that the underlying vector capacity is left unchanged (because most of the time we would have exactly the same number of items in the vector afer rebuilding the cache).
	 */
	void Clear()
	{
		this->data.ForEachElement([](LinearQueryTree<T>& tree) {
			tree.Clear();
		});
	}

	/**
	 * Resize the tree (may or may not preserve existing data).
	 */
	bool Resize(uint8 height, uint8 width)
	{
		if (this->data.Resize(height)) {
			this->data.ForEachElement([&](LinearQueryTree<T>& tree) {
				tree.Resize(width);
			});
			return true;
		}
		return false;
	}

	/**
	 * Emplaces a new item into the area query tree, associated with a given rectangle.  O(1).
	 * @param rect The rectangle associated with this item.
	 * @param args Arguments to use to construct the new item in-place.
	 */
	template <typename ...TArgs>
	T& Emplace(uint left, uint top, uint right, uint bottom, TArgs &&...args)
	{
		return this->data.Get(top, bottom).Emplace(left, right, std::forward<TArgs>(args)...);
	}

	/**
	 * Calls the given callback once per item in the area query tree; invokes callback(T) once per item.  O(K + R*C).
	 * @param rect The rectangle to query.
	 * @param callback Callback to invoke once per item.
	 */
	template <typename TCallback>
	void Query(uint left, uint top, uint right, uint bottom, TCallback callback)
	{
		this->data.Query(top, bottom, [&](LinearQueryTree<T>& tree) {
			/* Note: Can optimize by calculating `first_set` and `depth` for the LinearQueryTree, because all our LinearQueryTrees use the same  `first_set` and `depth` arguments. */
			tree.Query(left, right, callback);
		});
	}

	template <typename TCallback>
	void Query(uint left, uint top, uint right, uint bottom, TCallback callback) const
	{
		this->data.Query(top, bottom, [&](const LinearQueryTree<T>& tree) {
			/* Note: Can optimize by calculating `first_set` and `depth` for the LinearQueryTree, because all our LinearQueryTrees use the same  `first_set` and `depth` arguments. */
			tree.Query(left, right, callback);
		});
	}

};


#endif /* AREAQUERY_TYPE_HPP */
