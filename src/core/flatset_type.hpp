/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file flatset_type.hpp Flat set container implementation. */

#ifndef FLATSET_TYPE_HPP
#define FLATSET_TYPE_HPP

/**
 * Flat set implementation that uses a sorted vector for storage.
 * This is subset of functionality implemented by std::flat_set in c++23.
 * @tparam Tkey key type.
 * @tparam Tcompare key comparator.
 */
template <class Tkey, class Tcompare = std::less<>>
class FlatSet {
	std::vector<Tkey> data; ///< Sorted vector. of values.
public:
	using const_iterator = std::vector<Tkey>::const_iterator;

	/**
	 * Insert a key into the set.
	 * @param key Key to insert.
	 */
	void insert(const Tkey &key)
	{
		auto it = std::ranges::lower_bound(this->data, key, Tcompare{});
		if (it == std::end(this->data) || *it != key) this->data.emplace(it, key);
	}

	/**
	 * Erase a key from the set.
	 * @param key Key to erase.
	 * @return number of elements removed.
	 */
	size_t erase(const Tkey &key)
	{
		auto it = std::ranges::lower_bound(this->data, key, Tcompare{});
		if (it == std::end(this->data) || *it != key) return 0;

		this->data.erase(it);
		return 1;
	}

	/**
	 * Test if a key exists in the set.
	 * @param key Key to test.
	 * @return true iff the key exists in the set.
	 */
	bool contains(const Tkey &key)
	{
		return std::ranges::binary_search(this->data, key, Tcompare{});
	}

	const_iterator begin() const { return std::cbegin(this->data); }
	const_iterator end() const { return std::cend(this->data); }

	const_iterator cbegin() const { return std::cbegin(this->data); }
	const_iterator cend() const { return std::cend(this->data); }

	size_t size() const { return std::size(this->data); }
	bool empty() const { return this->data.empty(); }

	void clear() { this->data.clear(); }

	auto operator<=>(const FlatSet<Tkey, Tcompare> &) const = default;
};

#endif /* FLATSET_TYPE_HPP */
