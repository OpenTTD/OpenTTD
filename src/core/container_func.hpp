/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file container_func.hpp Functions related to use of containers. */

template <typename C, typename UP> unsigned int container_unordered_remove_if (C &container, UP predicate) {
	unsigned int removecount = 0;
	for (auto it = container.begin(); it != container.end();) {
		if (predicate(*it)) {
			removecount++;
			if (std::next(it) != container.end()) {
				*it = std::move(container.back());
				container.pop_back();
			} else {
				container.pop_back();
				break;
			}
		} else {
			++it;
		}
	}
	return removecount;
}

template <typename C, typename V> unsigned int container_unordered_remove(C &container, const V &value) {
	return container_unordered_remove_if (container, [&](const typename C::value_type &v) {
		return v == value;
	});
}
