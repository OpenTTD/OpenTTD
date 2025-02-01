/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file format.hpp String formatting functions and helpers. */

#ifndef FORMAT_HPP
#define FORMAT_HPP

#include "../3rdparty/fmt/format.h"
#include "convertible_through_base.hpp"

template <typename E, typename Char> requires std::is_enum_v<E>
struct fmt::formatter<E, Char> : fmt::formatter<typename std::underlying_type_t<E>> {
	using underlying_type = typename std::underlying_type_t<E>;
	using parent = typename fmt::formatter<underlying_type>;

	constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context &ctx)
	{
		return parent::parse(ctx);
	}

	fmt::format_context::iterator format(const E &e, fmt::format_context &ctx) const
	{
		return parent::format(underlying_type(e), ctx);
	}
};

template <ConvertibleThroughBase T, typename Char>
struct fmt::formatter<T, Char> : fmt::formatter<typename T::BaseType> {
	using underlying_type = typename T::BaseType;
	using parent = typename fmt::formatter<underlying_type>;

	constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context &ctx)
	{
		return parent::parse(ctx);
	}

	fmt::format_context::iterator format(const T &t, fmt::format_context &ctx) const
	{
		return parent::format(t.base(), ctx);
	}
};

#endif /* FORMAT_HPP */
