/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file autorelease.hpp Helper for std::unique_ptr to use an arbitrary function as the deleter. */

#ifndef AUTORELEASE_HPP
#define AUTORELEASE_HPP

/** Deleter that calls a function rather than deleting the pointer. */
template <auto Tfunc>
struct DeleterFromFunc {
	template <typename T>
	constexpr void operator()(T *arg) const
	{
		if (arg != nullptr) Tfunc(arg);
	}
};

/** Specialisation of std::unique_ptr for objects which must be deleted by calling a function. */
template <typename T, auto Tfunc>
using AutoRelease = std::unique_ptr<T, DeleterFromFunc<Tfunc>>;

#endif /* AUTORELEASE_HPP */
