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

#endif /* CONVERTIBLE_THROUGH_BASE_HPP */
