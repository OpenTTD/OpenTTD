/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file alloc_type.hpp Helper types related to the allocation of memory */

#ifndef ALLOC_TYPE_HPP
#define ALLOC_TYPE_HPP

/**
 * A reusable buffer that can be used for places that temporary allocate
 * a bit of memory and do that very often, or for places where static
 * memory is allocated that might need to be reallocated sometimes.
 *
 * Every time Allocate or ZeroAllocate is called previous results of both
 * functions will become invalid.
 */
template <typename T>
class ReusableBuffer {
private:
	std::vector<T> buffer;

public:
	/**
	 * Get buffer of at least count times T.
	 * @note the buffer might be bigger
	 * @note calling this function invalidates any previous buffers given
	 * @param count the minimum buffer size
	 * @return the buffer
	 */
	T *Allocate(size_t count)
	{
		if (this->buffer.size() < count) this->buffer.resize(count);
		return this->buffer.data();
	}

	/**
	 * Get buffer of at least count times T of default initialised elements.
	 * @note the buffer might be bigger
	 * @note calling this function invalidates any previous buffers given
	 * @param count the minimum buffer size
	 * @return the buffer
	 */
	T *ZeroAllocate(size_t count)
	{
		this->buffer.clear();
		this->buffer.resize(count, {});
		return this->buffer.data();
	}

	/**
	 * Get the currently allocated buffer.
	 * @return the buffer
	 */
	inline const T *GetBuffer() const
	{
		return this->buffer.data();
	}
};

#endif /* ALLOC_TYPE_HPP */
