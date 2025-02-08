/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file convertible_through_base.hpp Concept for unifying the convert through 'base()' behaviour of several 'strong' types. */

#ifndef CONVERTIBLE_THROUGH_BASE_HPP
#define CONVERTIBLE_THROUGH_BASE_HPP

/**
 * A type is considered 'convertible through base()' when it has a 'base()'
 * function that returns something that can be converted to int64_t.
 * @tparam T The type under consideration.
 */
template <typename T>
concept ConvertibleThroughBase = requires(T const a) {
	typename T::BaseType;
	{ a.base() } noexcept -> std::convertible_to<int64_t>;
};

/**
 * Type is convertible to TTo, either directly or through ConvertibleThroughBase.
 * @tparam T The type under consideration.
 * @tparam TTo The type to convert to.
 */
template <typename T, typename TTo>
concept ConvertibleThroughBaseOrTo = std::is_convertible_v<T, TTo> || ConvertibleThroughBase<T>;

/**
 * A sort-of mixin that adds 'at(pos)' and 'operator[](pos)' implementations for 'ConvertibleThroughBase' types.
 * This to prevent having to call '.base()' for many container accesses.
 */
template <typename Container>
class ReferenceThroughBaseContainer : public Container {
public:
	Container::reference at(ConvertibleThroughBase auto pos) { return this->Container::at(pos.base()); }
	Container::const_reference at(ConvertibleThroughBase auto pos) const { return this->Container::at(pos.base()); }

	Container::reference operator[](ConvertibleThroughBase auto pos) { return this->Container::operator[](pos.base()); }
	Container::const_reference operator[](ConvertibleThroughBase auto pos) const { return this->Container::operator[](pos.base()); }
};

#endif /* CONVERTIBLE_THROUGH_BASE_HPP */
