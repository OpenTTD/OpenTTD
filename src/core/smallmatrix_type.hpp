/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallmatrix_type.hpp Simple matrix class that allows allocating an item without the need to copy this->data needlessly. */

#ifndef SMALLMATRIX_TYPE_HPP
#define SMALLMATRIX_TYPE_HPP

#include "alloc_func.hpp"
#include "mem_func.hpp"

/**
 * Simple matrix template class.
 *
 * Allocating a matrix in one piece reduces overhead in allocations compared
 * with allocating a vector of vectors and saves some pointer dereferencing.
 * However, you can only get rectangular matrixes like this and if you're
 * changing their height very often performance will probably be worse than
 * with a vector of vectors, due to more frequent copying of memory blocks.
 *
 * No iterators are provided as iterating the columns would require persistent
 * column objects. Those do not exist. Providing iterators with transient
 * column objects would tie each iterator to a column object, thus replacing
 * previously retrieved columns when iterating and defeating the point of
 * iteration.
 *
 * It's expected that the items don't need to be constructed or deleted by the
 * container. Only memory allocation and deallocation is performed. This is the
 * same for all openttd "SmallContainer" classes.
 *
 * @tparam T The type of the items stored
 */
template <typename T>
class SmallMatrix {
protected:
	T *data;       ///< The pointer to the first item
	uint width;    ///< Number of items over first axis
	uint height;   ///< Number of items over second axis
	uint capacity; ///< The available space for storing items

public:

	SmallMatrix() : data(NULL), width(0), height(0), capacity(0) {}

	/**
	 * Copy constructor.
	 * @param other The other matrix to copy.
	 */
	SmallMatrix(const SmallMatrix &other) : data(NULL), width(0), height(0), capacity(0)
	{
		this->Assign(other);
	}

	~SmallMatrix()
	{
		free(this->data);
	}

	/**
	 * Assignment.
	 * @param other The other matrix to assign.
	 */
	SmallMatrix &operator=(const SmallMatrix &other)
	{
		this->Assign(other);
		return *this;
	}

	/**
	 * Assign items from other vector.
	 */
	inline void Assign(const SmallMatrix &other)
	{
		if (&other == this) return;

		this->height = other.Height();
		this->width = other.Width();
		uint num_items = this->width * this->height;
		if (num_items > this->capacity) {
			this->capacity = num_items;
			free(this->data);
			this->data = MallocT<T>(num_items);
			MemCpyT(this->data, other[0], num_items);
		} else if (num_items > 0) {
			MemCpyT(this->data, other[0], num_items);
		}
	}

	/**
	 * Remove all rows from the matrix.
	 */
	inline void Clear()
	{
		/* In fact we just reset the width avoiding the need to
		 * probably reallocate the same amount of memory the matrix was
		 * previously using. */
		this->width = 0;
	}

	/**
	 * Remove all items from the list and free allocated memory.
	 */
	inline void Reset()
	{
		this->height = 0;
		this->width = 0;
		this->capacity = 0;
		free(this->data);
		this->data = NULL;
	}

	/**
	 * Compact the matrix down to the smallest possible size.
	 */
	inline void Compact()
	{
		uint capacity = this->height * this->width;
		if (capacity >= this->capacity) return;
		this->capacity = capacity;
		this->data = ReallocT(this->data, this->capacity);
	}

	/**
	 * Erase a column, replacing it with the last one.
	 * @param x Position of the column.
	 */
	void EraseColumn(uint x)
	{
		if (x < --this->width) {
			MemCpyT<T>(this->data + x * this->height,
					this->data + this->width * this->height,
					this->height);
		}
	}

	/**
	 * Remove columns from the matrix while preserving the order of other columns.
	 * @param x First column to remove.
	 * @param count Number of consecutive columns to remove.
	 */
	void EraseColumnPreservingOrder(uint x, uint count = 1)
	{
		if (count == 0) return;
		assert(x < this->width);
		assert(x + count <= this->width);
		this->width -= count;
		uint to_move = (this->width - x) * this->height;
		if (to_move > 0) {
			MemMoveT(this->data + x * this->height,
					this->data + (x + count) * this->height, to_move);
		}
	}

	/**
	 * Erase a row, replacing it with the last one.
	 * @param y Position of the row.
	 */
	void EraseRow(uint y)
	{
		if (y < this->height - 1) {
			for (uint x = 0; x < this->width; ++x) {
				this->data[x * this->height + y] =
						this->data[(x + 1) * this->height - 1];
			}
		}
		this->Resize(this->width, this->height - 1);
	}

	/**
	 * Remove columns from the matrix while preserving the order of other columns.
	 * @param y First column to remove.
	 * @param count Number of consecutive columns to remove.
	 */
	void EraseRowPreservingOrder(uint y, uint count = 1)
	{
		if (this->height > count + y) {
			for (uint x = 0; x < this->width; ++x) {
				MemMoveT(this->data + x * this->height + y,
						this->data + x * this->height + y + count,
						this->height - count - y);
			}
		}
		this->Resize(this->width, this->height - count);
	}

	/**
	 * Append rows.
	 * @param to_add Number of rows to append.
	 */
	inline void AppendRow(uint to_add = 1)
	{
		this->Resize(this->width, to_add + this->height);
	}

	/**
	 * Append rows.
	 * @param to_add Number of rows to append.
	 */
	inline void AppendColumn(uint to_add = 1)
	{
		this->Resize(to_add + this->width, this->height);
	}

	/**
	 * Set the size to a specific width and height, preserving item positions
	 * as far as possible in the process.
	 * @param new_width Target width.
	 * @param new_height Target height.
	 */
	inline void Resize(uint new_width, uint new_height)
	{
		uint new_capacity = new_width * new_height;
		T *new_data = NULL;
		void (*copy)(T *dest, const T *src, size_t count) = NULL;
		if (new_capacity > this->capacity) {
			/* If the data doesn't fit into current capacity, resize and copy ... */
			new_data = MallocT<T>(new_capacity);
			copy = &MemCpyT<T>;
		} else {
			/* ... otherwise just move the columns around, if necessary. */
			new_data = this->data;
			copy = &MemMoveT<T>;
		}
		if (this->height != new_height || new_data != this->data) {
			if (this->height > 0) {
				if (new_height > this->height) {
					/* If matrix is growing, copy from the back to avoid
					 * overwriting uncopied data. */
					for (uint x = this->width; x > 0; --x) {
						if (x * new_height > new_capacity) continue;
						(*copy)(new_data + (x - 1) * new_height,
								this->data + (x - 1) * this->height,
								min(this->height, new_height));
					}
				} else {
					/* If matrix is shrinking copy from the front. */
					for (uint x = 0; x < this->width; ++x) {
						if ((x + 1) * new_height > new_capacity) break;
						(*copy)(new_data + x * new_height,
								this->data + x * this->height,
								min(this->height, new_height));
					}
				}
			}
			this->height = new_height;
			if (new_data != this->data) {
				free(this->data);
				this->data = new_data;
				this->capacity = new_capacity;
			}
		}
		this->width = new_width;
	}

	inline uint Height() const
	{
		return this->height;
	}

	inline uint Width() const
	{
		return this->width;
	}

	/**
	 * Get item x/y (const).
	 *
	 * @param x X-position of the item.
	 * @param y Y-position of the item.
	 * @return Item at specified position.
	 */
	inline const T &Get(uint x, uint y) const
	{
		assert(x < this->width && y < this->height);
		return this->data[x * this->height + y];
	}

	/**
	 * Get item x/y.
	 *
	 * @param x X-position of the item.
	 * @param y Y-position of the item.
	 * @return Item at specified position.
	 */
	inline T &Get(uint x, uint y)
	{
		assert(x < this->width && y < this->height);
		return this->data[x * this->height + y];
	}

	/**
	 * Get column "number" (const)
	 *
	 * @param x Position of the column.
	 * @return Column at "number".
	 */
	inline const T *operator[](uint x) const
	{
		assert(x < this->width);
		return this->data + x * this->height;
	}

	/**
	 * Get column "number" (const)
	 *
	 * @param x Position of the column.
	 * @return Column at "number".
	 */
	inline T *operator[](uint x)
	{
		assert(x < this->width);
		return this->data + x * this->height;
	}
};

#endif /* SMALLMATRIX_TYPE_HPP */
