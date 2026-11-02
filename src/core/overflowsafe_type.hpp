/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file overflowsafe_type.hpp An overflow safe integer-like type. */

#ifndef OVERFLOWSAFE_TYPE_HPP
#define OVERFLOWSAFE_TYPE_HPP

#include "math_func.hpp"


#ifdef __has_builtin
#	if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_sub_overflow) && __has_builtin(__builtin_mul_overflow)
#		define HAS_OVERFLOW_BUILTINS
#	endif
#endif

/**
 * Overflow safe template for integers, i.e. integers that will never overflow
 * when you multiply the maximum value with 2, or add 2, or subtract something
 * from the minimum value, etc.
 * @param T the type these integers are stored with.
 */
template <class T>
class OverflowSafeInt {
private:
	static constexpr T T_MAX = std::numeric_limits<T>::max(); ///< Maximum value of this type.
	static constexpr T T_MIN = std::numeric_limits<T>::min(); ///< Minimum value of this type.

	/**
	 * Convert from any integral type to the base type.
	 * @note if the type cannot be converted safely then a static_assert will be triggered.
	 * @param value The value to be converted.
	 * @return The value converted to the base type.
	 */
	static constexpr T ConvertIfSafe(std::integral auto value)
	{
		/* T must be larger than value or the same size and the same sign. */
		static_assert(sizeof(T) > sizeof(value) || (sizeof(T) == sizeof(value) && std::numeric_limits<T>::is_signed == std::numeric_limits<decltype(value)>::is_signed));
		return static_cast<T>(value);
	}

	/** The non-overflow safe backend to store the value in. */
	T m_value{};
public:
	using BaseType = T;

	/**
	 * Construct an overflow-safe type instance with a default value.
	 */
	constexpr OverflowSafeInt() = default;

	/**
	 * Construct an overflow-safe type from an integral type.
	 * @param value The value.
	 */
	constexpr OverflowSafeInt(std::integral auto value) : m_value(ConvertIfSafe(value)) {}

	/**
	 * Safely construct an overflow-safe type from an overflow-safe type with a different base type.
	 * @note if the type cannot be converted safetly then a static_assert will be triggered.
	 * @tparam Tother The base type of the original type.
	 * @param value The original overflow-safe type.
	 */
	template <class Tother>
	constexpr OverflowSafeInt(const OverflowSafeInt<Tother> &value) : m_value(ConvertIfSafe(value.base())) {}

	/**
	 * Safely assign an integral value to this overflow-safe instance.
	 * @param other The value to assign.
	 * @return This instance.
	 */
	constexpr OverflowSafeInt &operator=(std::integral auto other) { this->m_value = ConvertIfSafe(other); return *this; }

	constexpr OverflowSafeInt operator-() const { return OverflowSafeInt{this->m_value == T_MIN ? T_MAX : -this->m_value}; }

	/**
	 * Safe implementation of addition.
	 * @param other the amount to add
	 * @return Reference to this instance.
	 * @note when the addition would yield more than T_MAX, it will be T_MAX.
	 */
	constexpr OverflowSafeInt &operator+=(const OverflowSafeInt &other)
	{
#ifdef HAS_OVERFLOW_BUILTINS
		if (__builtin_add_overflow(this->m_value, other.m_value, &this->m_value)) [[unlikely]] {
			this->m_value = (other.m_value < 0) ? T_MIN : T_MAX;
		}
#else
		if (this->m_value > 0 && other.m_value > 0 && (T_MAX - other.m_value) < this->m_value) {
			this->m_value = T_MAX;
		} else if (this->m_value < 0 && other.m_value < 0 && (this->m_value == T_MIN || other.m_value == T_MIN || ((T_MAX + this->m_value) + other.m_value < (T_MIN + T_MAX)))) {
			this->m_value = T_MIN;
		} else {
			this->m_value += other.m_value;
		}
#endif
		return *this;
	}

	/**
	 * Safe implementation of subtraction.
	 * @param other the amount to subtract
	 * @return Reference to this instance.
	 * @note when the subtraction would yield less than T_MIN, it will be T_MIN.
	 */
	constexpr OverflowSafeInt &operator-=(const OverflowSafeInt &other)
	{
#ifdef HAS_OVERFLOW_BUILTINS
		if (__builtin_sub_overflow(this->m_value, other.m_value, &this->m_value)) [[unlikely]] {
			this->m_value = (other.m_value < 0) ? T_MAX : T_MIN;
		}
#else
		if (this->m_value > 0 && other.m_value < 0 && (T_MAX + other.m_value) < this->m_value) {
			this->m_value = T_MAX;
		} else if (this->m_value < 0 && other.m_value > 0 && (T_MAX + this->m_value) < (T_MIN + T_MAX) + other.m_value) {
			this->m_value = T_MIN;
		} else {
			this->m_value -= other.m_value;
		}
#endif
		return *this;
	}

	/** @{
	 * Safe add operator.
	 * @param other The other value to add.
	 * @return The result of the addition.
	 */
	constexpr OverflowSafeInt operator+(const OverflowSafeInt &other) const { OverflowSafeInt result = *this; result += other; return result; }
	constexpr OverflowSafeInt operator+(std::integral auto other) const { OverflowSafeInt result = *this; result += other; return result; }
	/** @} */

	/** @{
	 * Safe subtract operator.
	 * @param other The other value to subtract.
	 * @return The result of the subtraction.
	 */
	constexpr OverflowSafeInt operator-(const OverflowSafeInt &other) const { OverflowSafeInt result = *this; result -= other; return result; }
	constexpr OverflowSafeInt operator-(std::integral auto other) const { OverflowSafeInt result = *this; result -= other; return result; }
	/** @} */

	constexpr OverflowSafeInt &operator++() { return *this += 1; }
	constexpr OverflowSafeInt operator++(int) { OverflowSafeInt org = *this; *this += 1; return org; }
	constexpr OverflowSafeInt &operator--() { return *this += -1; }
	constexpr OverflowSafeInt operator--(int) { OverflowSafeInt org = *this; *this += -1; return org; }

	/**
	 * Safe implementation of multiplication.
	 * @param factor the factor to multiply this with.
	 * @return Reference to this instance.
	 * @note when the multiplication would yield more than T_MAX (or less than T_MIN),
	 *       it will be T_MAX (respectively T_MIN).
	 */
	constexpr OverflowSafeInt &operator*=(const OverflowSafeInt &factor)
	{
#ifdef HAS_OVERFLOW_BUILTINS
		const bool is_result_positive = (this->m_value < 0) == (factor.m_value < 0); // -ve * -ve == +ve
		if (__builtin_mul_overflow(this->m_value, factor.m_value, &this->m_value)) [[unlikely]] {
			this->m_value = is_result_positive ? T_MAX : T_MIN;
		}
#else
		if (factor.m_value == -1) {
			this->m_value = (this->m_value == T_MIN) ? T_MAX : -this->m_value;
		} else if (factor.m_value > 0 && this->m_value > 0 && (T_MAX / factor.m_value) < this->m_value) {
			this->m_value = T_MAX;
		} else if (factor.m_value > 0 && this->m_value < 0 && (T_MIN / factor.m_value) > this->m_value) {
			this->m_value = T_MIN;
		} else if (factor.m_value < 0 && this->m_value > 0 && (T_MIN / factor.m_value) < this->m_value) {
			this->m_value = T_MIN;
		} else if (factor.m_value < 0 && this->m_value < 0 && (T_MAX / factor.m_value) > this->m_value) {
			this->m_value = T_MAX;
		} else {
			this->m_value *= factor.m_value;
		}
#endif
		return *this;
	}

	/** @{
	 * Safely multiply operator.
	 * @param factor The factor to mulitply with.
	 * @return The result of the multiplication.
	 */
	constexpr OverflowSafeInt operator*(const OverflowSafeInt &factor) const { OverflowSafeInt result = *this; result *= factor; return result; }
	constexpr OverflowSafeInt operator*(std::integral auto factor) const { OverflowSafeInt result = *this; result *= factor; return result; }
	/** @} */

	/**
	 * Divide operator.
	 * @param divisor The divisor to divide with.
	 * @return This instance.
	 */
	constexpr OverflowSafeInt &operator/=(const OverflowSafeInt &divisor) { this->m_value /= divisor.m_value; return *this; }

	/** @{
	 * Divide operator.
	 * @param divisor The divisor to divide with.
	 * @return The result of the division.
	 */
	constexpr OverflowSafeInt operator/(const OverflowSafeInt &divisor) const { OverflowSafeInt result = *this; result /= divisor; return result; }
	constexpr OverflowSafeInt operator/(std::integral auto divisor) const { OverflowSafeInt result = *this; result /= divisor; return result; }
	/** @} */

	/**
	 * Modulo operator.
	 * @param divisor The divisor to modulo with.
	 * @return This instance.
	 */
	constexpr OverflowSafeInt &operator%=(const OverflowSafeInt &divisor) { this->m_value %= divisor.m_value; return *this; }

	/** @{
	 * Modulo operator.
	 * @param divisor The divisor to modulo with.
	 * @return The result of the modulo.
	 */
	constexpr OverflowSafeInt operator%(const OverflowSafeInt &divisor) const { OverflowSafeInt result = *this; result %= divisor; return result; }
	constexpr OverflowSafeInt operator%(std::integral auto divisor) const { OverflowSafeInt result = *this; result %= divisor; return result; }
	/** @} */

	/* Operators for shifting. */
	constexpr OverflowSafeInt &operator<<=(const int shift) { this->m_value <<= shift; return *this; }
	constexpr OverflowSafeInt operator<<(const int shift) const { OverflowSafeInt result = *this; result <<= shift; return result; }
	constexpr OverflowSafeInt &operator>>=(const int shift) { this->m_value >>= shift; return *this; }
	constexpr OverflowSafeInt operator>>(const int shift) const { OverflowSafeInt result = *this; result >>= shift; return result; }

	/** @{
	 * Equality comparison operator.
	 * @param other The other value to compare with.
	 * @return \c true if the other value is the same as this value..
	 */
	constexpr bool operator==(const OverflowSafeInt &other) const { return this->m_value == other.m_value; }
	constexpr bool operator==(std::integral auto other) const { return this->m_value == ConvertIfSafe(other); }
	/** @} */

	/** @{
	 * Comparison operator.
	 * @param other The other value to compare with.
	 * @return The comparison result.
	 */
	constexpr auto operator<=>(const OverflowSafeInt &other) const { return this->m_value <=> other.m_value; }
	constexpr auto operator<=>(std::integral auto other) const { return this->m_value <=> ConvertIfSafe(other); }
	/** @} */

	constexpr operator T() const { return this->m_value; }

	static constexpr OverflowSafeInt<T> max() { return T_MAX; }
	static constexpr OverflowSafeInt<T> min() { return T_MIN; }

	/**
	 * Get the underlying value of this instance.
	 * @return The underlying value.
	 */
	constexpr BaseType base() const noexcept { return this->m_value; }

	/** @{
	 * Operators for when an OverflowSafeInt is on the right-hand-side of an operation with an integral value.
	 * @param lhs the integral value.
	 * @param rhs the overflow-safe value.
	 * @return The result of the operation.
	 */
	friend constexpr OverflowSafeInt operator+(std::integral auto lhs, const OverflowSafeInt &rhs) noexcept { return OverflowSafeInt{lhs} + rhs; }
	friend constexpr OverflowSafeInt operator-(std::integral auto lhs, const OverflowSafeInt &rhs) noexcept { return OverflowSafeInt{lhs} - rhs; }
	friend constexpr OverflowSafeInt operator*(std::integral auto lhs, const OverflowSafeInt &rhs) noexcept { return OverflowSafeInt{lhs} * rhs; }
	friend constexpr OverflowSafeInt operator/(std::integral auto lhs, const OverflowSafeInt &rhs) noexcept { return OverflowSafeInt{lhs} / rhs; }
	friend constexpr OverflowSafeInt operator%(std::integral auto lhs, const OverflowSafeInt &rhs) noexcept { return OverflowSafeInt{lhs} % rhs; }
	/** @} */
};

/** A 64-bit overflow safe type. */
using OverflowSafeInt64 = OverflowSafeInt<int64_t>;

/** A 32-bit overflow safe type. */
using OverflowSafeInt32 = OverflowSafeInt<int32_t>;

/* Some basic "unit tests". Also has the bonus of confirming that constexpr is working. */
static_assert(OverflowSafeInt32(INT32_MIN) - 1 == OverflowSafeInt32(INT32_MIN));
static_assert(OverflowSafeInt32(INT32_MAX) + 1 == OverflowSafeInt32(INT32_MAX));
static_assert(OverflowSafeInt32(INT32_MAX) * 2 == OverflowSafeInt32(INT32_MAX));
static_assert(OverflowSafeInt32(INT32_MIN) * 2 == OverflowSafeInt32(INT32_MIN));

/* Specialisation of the generic ClampTo function for overflow safe integers to normal integers. */
template <typename To, typename From>
constexpr To ClampTo(OverflowSafeInt<From> value) { return ClampTo<To>(value.base()); }

#undef HAS_OVERFLOW_BUILTINS

#endif /* OVERFLOWSAFE_TYPE_HPP */
