/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strong_typedef_type.hpp Type (helpers) for making a strong typedef that is a distinct type. */

#ifndef STRONG_TYPEDEF_TYPE_HPP
#define STRONG_TYPEDEF_TYPE_HPP

/** Non-templated base for #StrongTypedef for use with type trait queries. */
struct StrongTypedefBase {};

/**
 * Templated helper to make a type-safe 'typedef' representing a single POD value.
 * A normal 'typedef' is not distinct from its base type and will be treated as
 * identical in many contexts. This class provides a distinct type that can still
 * be assign from and compared to values of its base type.
 *
 * @note This is meant to be used as a base class, not directly.
 * @tparam T Storage type
 * @tparam Tthis Type of the derived class (i.e. the concrete usage of this class).
 */
template <class T, class Tthis>
struct StrongTypedef : StrongTypedefBase {
	using Type = T;

	T value{}; ///< Backing storage field.

	constexpr StrongTypedef() = default;
	constexpr StrongTypedef(const StrongTypedef &o) = default;
	constexpr StrongTypedef(StrongTypedef &&o) = default;

	constexpr StrongTypedef(const T &value) : value(value) {}

	constexpr Tthis &operator =(const StrongTypedef &rhs) { this->value = rhs.value; return static_cast<Tthis &>(*this); }
	constexpr Tthis &operator =(StrongTypedef &&rhs) { this->value = std::move(rhs.value); return static_cast<Tthis &>(*this); }
	constexpr Tthis &operator =(const T &rhs) { this->value = rhs; return static_cast<Tthis &>(*this); }

	explicit constexpr operator T() const { return this->value; }

	constexpr bool operator ==(const StrongTypedef &rhs) const { return this->value == rhs.value; }
	constexpr bool operator !=(const StrongTypedef &rhs) const { return this->value != rhs.value; }
	constexpr bool operator ==(const T &rhs) const { return this->value == rhs; }
	constexpr bool operator !=(const T &rhs) const { return this->value != rhs; }
};

/**
 * Extension of #StrongTypedef with operators for addition and subtraction.
 * @tparam T Storage type
 * @tparam Tthis Type of the derived class (i.e. the concrete usage of this class).
 */
template <class T, class Tthis>
struct StrongIntegralTypedef : StrongTypedef<T, Tthis> {
	using StrongTypedef<T, Tthis>::StrongTypedef;

	constexpr Tthis &operator ++() { this->value++; return static_cast<Tthis &>(*this); }
	constexpr Tthis &operator --() { this->value--; return static_cast<Tthis &>(*this); }
	constexpr Tthis operator ++(int) { auto res = static_cast<Tthis &>(*this); this->value++; return res; }
	constexpr Tthis operator --(int) { auto res = static_cast<Tthis &>(*this); this->value--; return res; }

	constexpr Tthis &operator +=(const Tthis &rhs) { this->value += rhs.value; return *static_cast<Tthis *>(this); }
	constexpr Tthis &operator -=(const Tthis &rhs) { this->value -= rhs.value; return *static_cast<Tthis *>(this); }

	constexpr Tthis operator +(const Tthis &rhs) const { return Tthis{ this->value + rhs.value }; }
	constexpr Tthis operator -(const Tthis &rhs) const { return Tthis{ this->value - rhs.value }; }
	constexpr Tthis operator +(const T &rhs) const { return Tthis{ this->value + rhs }; }
	constexpr Tthis operator -(const T &rhs) const { return Tthis{ this->value - rhs }; }
};

#endif /* STRONG_TYPEDEF_TYPE_HPP */
