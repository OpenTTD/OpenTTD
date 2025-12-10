/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
 * A sort-of mixin that implements 'at(pos)' and 'operator[](pos)' only for a specific type.
 * The type must have a suitable '.base()' method and therefore must inherently match 'ConvertibleThroughBase'.
 * This to prevent having to call '.base()' for many container accesses, whilst preventing accidental use of the wrong index type.
 */
template <typename Container, typename Index>
class TypedIndexContainer : public Container {
public:
	Container::reference at(size_t pos) { return this->Container::at(pos); }
	Container::reference at(const Index &pos) { return this->Container::at(pos.base()); }

	Container::const_reference at(size_t pos) const { return this->Container::at(pos); }
	Container::const_reference at(const Index &pos) const { return this->Container::at(pos.base()); }

	Container::reference operator[](size_t pos) { return this->Container::operator[](pos); }
	Container::reference operator[](const Index &pos) { return this->Container::operator[](pos.base()); }

	Container::const_reference operator[](size_t pos) const { return this->Container::operator[](pos); }
	Container::const_reference operator[](const Index &pos) const { return this->Container::operator[](pos.base()); }
};

#endif /* CONVERTIBLE_THROUGH_BASE_HPP */
