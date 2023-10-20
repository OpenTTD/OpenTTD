/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file span_type.hpp Minimized implementation of C++20 std::span. */

#ifndef CORE_SPAN_TYPE_HPP
#define CORE_SPAN_TYPE_HPP

/* This is a partial copy/paste from https://github.com/gsl-lite/gsl-lite/blob/master/include/gsl/gsl-lite.hpp */

/* Template to check if a template variable defines size() and data(). */
template <class, class = void>
struct has_size_and_data : std::false_type{};
template <class C>
struct has_size_and_data
<
	C, std::void_t<
		decltype(std::size(std::declval<C>())),
		decltype(std::data(std::declval<C>()))>
> : std::true_type{};

/* Template to check if two elements are compatible. */
template <class, class, class = void>
struct is_compatible_element : std::false_type {};
template <class C, class E>
struct is_compatible_element
<
	C, E, std::void_t<
		decltype(std::data(std::declval<C>())),
		typename std::remove_pointer_t<decltype(std::data( std::declval<C&>()))>(*)[]>
> : std::is_convertible<typename std::remove_pointer_t<decltype(std::data(std::declval<C&>()))>(*)[], E(*)[]>{};

/* Template to check if a container is compatible. gsl-lite also includes is_array and is_std_array, but as we don't use them, they are omitted. */
template <class C, class E>
struct is_compatible_container : std::bool_constant
<
	has_size_and_data<C>::value
	&& is_compatible_element<C,E>::value
>{};

/**
 * A trimmed down version of what std::span will be in C++20.
 *
 * It is fully forwards compatible, so if this codebase switches to C++20,
 * all "span" instances can be replaced by "std::span" without loss of
 * functionality.
 *
 * Currently it only supports basic functionality:
 * - size() and friends
 * - begin() and friends
 *
 * It is meant to simplify function parameters, where we only want to walk
 * a continuous list.
 */
template<class T>
class span {
public:
	typedef T element_type;
	typedef typename std::remove_cv< T >::type value_type;

	typedef T &reference;
	typedef T *pointer;
	typedef const T &const_reference;
	typedef const T *const_pointer;

	typedef pointer iterator;
	typedef const_pointer const_iterator;

	typedef size_t size_type;
	typedef std::ptrdiff_t difference_type;

	constexpr span() noexcept : first(nullptr), last(nullptr) {}

	constexpr span(pointer data_in, size_t size_in) : first(data_in), last(data_in + size_in) {}

	template<class Container, typename std::enable_if<(is_compatible_container<Container, element_type>::value), int>::type = 0>
	constexpr span(Container &list) noexcept : first(std::data(list)), last(std::data(list) + std::size(list)) {}
	template<class Container, typename std::enable_if<(std::is_const<element_type>::value && is_compatible_container<Container, element_type>::value), int>::type = 0>
	constexpr span(const Container &list) noexcept : first(std::data(list)), last(std::data(list) + std::size(list)) {}

	constexpr size_t size() const noexcept { return static_cast<size_t>( last - first ); }
	constexpr std::ptrdiff_t ssize() const noexcept { return static_cast<std::ptrdiff_t>( last - first ); }
	constexpr bool empty() const noexcept { return this->size() == 0; }

	constexpr iterator begin() const noexcept { return iterator(first); }
	constexpr iterator end() const noexcept { return iterator(last); }

	constexpr const_iterator cbegin() const noexcept { return const_iterator(first); }
	constexpr const_iterator cend() const noexcept { return const_iterator(last); }

	constexpr reference operator[](size_type idx) const { return first[idx]; }

	constexpr pointer data() const noexcept { return first; }

	constexpr span<element_type> subspan(size_t offset, size_t count)
	{
		assert(offset + count <= size());
		return span(this->data() + offset, count);
	}

private:
	pointer first;
	pointer last;
};

#endif /* CORE_SPAN_TYPE_HPP */
