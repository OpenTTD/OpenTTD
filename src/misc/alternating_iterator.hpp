/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file alternating_iterator.hpp Iterator adaptor that takes items alternating from a middle position. */

#ifndef ALTERNATING_ITERATOR_HPP
#define ALTERNATING_ITERATOR_HPP

#include <ranges>

/**
 * Iterator that alternately takes from the "middle" of a range.
 * @tparam Titer Type of iterator.
 */
template <typename Titer>
class AlternatingIterator {
public:
	using value_type = typename Titer::value_type;
	using difference_type = std::ptrdiff_t;
	using iterator_category = std::forward_iterator_tag;
	using pointer = typename Titer::pointer;
	using reference = typename Titer::reference;

	AlternatingIterator() = default;

	/**
	 * Construct an AlternatingIterator.
	 * @param first Iterator to first element.
	 * @param last Iterator to last element.
	 * @param middle Iterator to "middle" element, from where to start.
	 * @param begin Whether this iterator points to the first or last elements.
	 */
	AlternatingIterator(Titer first, Titer last, Titer middle, bool begin) : first(first), last(last), middle(middle)
	{
		/* Starting from the end is not supported, unless the range is empty. */
		assert(first == last || middle != last);

		this->position = begin ? 0 : std::distance(this->first, this->last);
		this->before = middle;
		this->after = middle;
		this->next_state = this->before == this->first;
		this->state = this->next_state;
	}

	bool operator==(const AlternatingIterator &rhs) const
	{
		assert(this->first == rhs.first);
		assert(this->last == rhs.last);
		assert(this->middle == rhs.middle);
		return this->position == rhs.position;
	}

	std::strong_ordering operator<=>(const AlternatingIterator &rhs) const
	{
		assert(this->first == rhs.first);
		assert(this->last == rhs.last);
		assert(this->middle == rhs.middle);
		return this->position <=> rhs.position;
	}

	inline reference operator*() const
	{
		return *this->Base();
	}

	AlternatingIterator &operator++()
	{
		size_t size = static_cast<size_t>(std::distance(this->first, this->last));
		assert(this->position < size);

		++this->position;
		if (this->position < size) this->Next();

		return *this;
	}

	AlternatingIterator operator++(int)
	{
		AlternatingIterator result = *this;
		++*this;
		return result;
	}

	inline Titer Base() const
	{
		return this->state ? this->after : this->before;
	}

private:
	Titer first; ///< Initial first iterator.
	Titer last; ///< Initial last iterator.
	Titer middle; ///< Initial middle iterator.

	Titer after; ///< Current iterator after the middle.
	Titer before; ///< Current iterator before the middle.

	size_t position; ///< Position within the entire range.

	bool next_state; ///< Next state for advancing iterator. If true take from after middle, otherwise take from before middle.
	bool state; ///< Current state for reading iterator. If true take from after middle, otherwise take from before middle.

	void Next()
	{
		this->state = this->next_state;
		if (this->next_state) {
			assert(this->after != this->last);
			++this->after;
			this->next_state = this->before == this->first;
		} else {
			assert(this->before != this->first);
			--this->before;
			this->next_state = std::next(this->after) != this->last;
		}
	}
};

template <typename Titer>
class AlternatingView : public std::ranges::view_interface<AlternatingIterator<Titer>> {
public:
	AlternatingView(std::ranges::viewable_range auto &&range, Titer middle) :
		first(std::ranges::begin(range)), last(std::ranges::end(range)), middle(middle)
	{
	}

	auto begin() const
	{
		return AlternatingIterator{first, last, middle, true};
	}

	auto end() const
	{
		return AlternatingIterator{first, last, middle, false};
	}

private:
	Titer first; ///< Iterator to first element.
	Titer last; ///< Iterator to last element.
	Titer middle; ///< Iterator to middle element.
};

#endif /* ALTERNATING_ITERATOR_HPP */
