/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file linearquery_type.hpp 1D segment tree class; O(1) insertion of area, O(K + L) retrieval of items within a given area (where K is the number of items in an area at most 2x the size of the given area, and L is the size of the subrange being queried respectively). */

#ifndef LINEARQUERY_TYPE_HPP
#define LINEARQUERY_TYPE_HPP

#include "bitmath_func.hpp"

#include <memory>
#include <utility>
#include <vector>

/**
 * Segment tree class that can do efficient enumeration of segments that intersect a given range.
 * Note: In all functions, `end` is inclusive of that element itself, i.e. the range is [begin, end] and NOT [begin, end).
 *
 * @tparam T The type of the items stored
 */
template <typename T>
class SegmentTree {
private:
	uint8 size;
	std::unique_ptr<T[]> data; // data[1] is the full range, data[2-3] are the two half-ranges, data[4-7] are the four quarter-ranges, etc.
	enum : uint8 {
		INVALID_SIZE = static_cast<uint8>(-1)
	};

public:
	SegmentTree() : size(INVALID_SIZE) {}
	SegmentTree(const SegmentTree&) = delete;
	SegmentTree& operator=(const SegmentTree&) = delete;
	SegmentTree(SegmentTree&& other) : size(other.size), data(std::move(other.data))
	{
		other.size = INVALID_SIZE;
		other.data = nullptr;
	}
	SegmentTree& operator=(SegmentTree&& other)
	{
		this->size = other.size;
		this->data = std::move(other.data);
		other.size = INVALID_SIZE;
		other.data = nullptr;
	}

	/**
	 * Constructs an segment tree with the given size.
	 * @param size The size of the tree (expressed as a power of two, i.e. 2^(size) is the number of distinct positions in the line).
	 */
	SegmentTree(uint8 size) : size(size)
	{
		assert(this->size != INVALID_SIZE);
		this->data = std::make_unique<T[]>(static_cast<std::size_t>(1) << (size + 1));
	}

	/**
	 * Resizes the tree.
	 * @param size The desired size.
	 * @return True if the size was actually changed (if the size is unchanged, then existing data is guaranteed to be preserved).
	 */
	bool Resize(uint8 size)
	{
		assert(size != INVALID_SIZE);
		if (this->size != size) {
			this->data = std::make_unique<T[]>(static_cast<std::size_t>(1) << (size + 1));
			this->size = size;
			return true;
		}
		return false;
	}

	/**
	 * Invokes callback(T) for each element of the segment tree (in arbitrary order).
	 */
	template <typename TCallback>
	void ForEachElement(TCallback callback) const
	{
		const uint real_size = (1u << (this->size + 1));
		for (uint i = 1; i < real_size; ++i) {
			assert(i > 0 && i < (1u << (this->size + 1)));
			callback(this->data[i]);
		}
	}

	/**
	 * Resolves the index into the data array, associated with a given range.  O(1) assuming FindLastBit is O(1).
	 * @param begin The start of the range.
	 * @param end The end of the range.
	 * @return The desired index into the data array.
	 */
	uint ResolveDataIndex(uint begin, uint end)
	{
		assert(this->size != INVALID_SIZE);
		assert(begin <= end);
		assert(end < (1u << this->size));

		/* Find the position (from LSB) of the first bit that differs; this determines the depth of the chosen range. */
		const uint8 first_set = FindLastBit(begin ^ end) + 1;

		/* Find the depth in the tree, 1-based.  '1' means the full range, '2' means the half-range, etc. */
		const uint8 depth = this->size - first_set;

		/* Find the index into the data that is chosen to store this new item. */
		const uint data_index = (1u << depth) + (begin >> first_set);

		return data_index;
	}

	/**
	 * Gets the element at that particular index.
	 * @param data_index The index into the data array.
	 * @return A reference to the element at that particular index.
	 */
	T& Get(uint data_index)
	{
		assert(this->size != INVALID_SIZE);
		assert(data_index > 0 && data_index < (1u << (this->size + 1)));

		return this->data[data_index];
	}

	/**
	 * Gets the element associated with a given range.  O(1) assuming FindLastBit is O(1).
	 * @param begin The start of the range.
	 * @param end The end of the range.
	 * @return A reference to the element at that particular index.
	 */
	T& Get(uint begin, uint end)
	{
		return Get(ResolveDataIndex(begin, end));
	}

	/**
	 * Calls the given callback once per element in the segment tree; invokes callback(T) once per element.  O(K + L).
	 * It might also call the callback on other random elements that are outside the range, so the caller should ascertain that each called element is indeed within the desired range.
	 * @param begin The start of the range.
	 * @param end The end of the range.
	 * @param callback Callback to invoke once per element.
	 */
	template <typename TCallback>
	void Query(uint begin, uint end, TCallback callback)
	{
		assert(this->size != INVALID_SIZE);
		assert(begin <= end);
		assert(end < (1u << this->size));

		for (uint8 depth = 0; depth <= this->size; ++depth) {
			const uint8 stride = this->size - depth;

			const uint real_end = (1u << depth) + (end >> stride);

			for (uint i = (1u << depth) + (begin >> stride); i <= real_end; ++i) {
				assert(i > 0 && i < (1u << (this->size + 1)));
				callback(this->data[i]);
			}
		}
	}

	template <typename TCallback>
	void Query(uint begin, uint end, TCallback callback) const
	{
		assert(this->size != INVALID_SIZE);
		assert(begin <= end);
		assert(end < (1u << this->size));

		for (uint8 depth = 0; depth <= this->size; ++depth) {
			const uint8 stride = this->size - depth;

			const uint real_end = (1u << depth) + (end >> stride);

			for (uint i = (1u << depth) + (begin >> stride); i <= real_end; ++i) {
				assert(i > 0 && i < (1u << (this->size + 1)));
				callback(const_cast<const T&>(this->data[i]));
			}
		}
	}


};

/**
 * Linear query template class.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @tparam T The type of the items stored
 */
template <typename T>
class LinearQueryTree {
private:
	SegmentTree<std::vector<T>> data;

public:
	LinearQueryTree() {}

	/**
	 * Constructs an empty linear query tree with the given size.
	 * @param size The size of the tree (expressed as a power of two, i.e. 2^(size) is the number of distinct positions in the line).
	 */
	LinearQueryTree(uint8 size) : data(size) {}

	/**
	 * Clear all the data from the tree.  Note: It can be made faster by ensuring that the underlying vector capacity is left unchanged (because most of the time we would have exactly the same number of items in the vector afer rebuilding the cache).
	 */
	void Clear()
	{
		this->data.ForEachElement([](std::vector<T>& vec) {
			vec.clear();
		});
	}

	/**
	 * Resizes the tree.
	 * @param size The desired size.
	 * @return True if the size was actually changed (if the size is unchanged, then existing data is guaranteed to be preserved).
	 */
	bool Resize(uint8 size)
	{
		return this->data.Resize(size);
	}

	/**
	 * Emplaces a new item into the linear query tree, associated with a given range.  O(1) assuming FindLastBit is O(1).
	 * @param begin The start of the range.
	 * @param end The end of the range.
	 * @param args Arguments to use to construct the new item in-place.
	 */
	template <typename ...TArgs>
	T& Emplace(uint begin, uint end, TArgs &&...args)
	{
		std::vector<T>& vec = this->data.Get(begin, end);
		vec.emplace_back(std::forward<TArgs>(args)...);
		return vec.back();
	}

	/**
	 * Calls the given callback once per item in the linear query tree; invokes callback(T) once per item.  O(K + L).
	 * It might also call the callback on other random items that are outside the range, so the caller should ascertain that each called item is indeed within the desired range.
	 * @param begin The start of the range.
	 * @param end The end of the range.
	 * @param callback Callback to invoke once per item.
	 */
	template <typename TCallback>
	void Query(uint begin, uint end, TCallback callback)
	{
		this->data.Query(begin, end, [&callback](std::vector<T>& vec) {
			for (T& item : vec) {
				callback(item);
			}
		});
	}

	template <typename TCallback>
	void Query(uint begin, uint end, TCallback callback) const
	{
		this->data.Query(begin, end, [&callback](const std::vector<T>& vec) {
			for (const T& item : vec) {
				callback(item);
			}
		});
	}


};


#endif /* LINEARQUERY_TYPE_HPP */
