/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sortlist_type.h Base types for having sorted lists in GUIs. */

#ifndef SORTLIST_TYPE_H
#define SORTLIST_TYPE_H

#include "core/enum_type.hpp"
#include "core/bitmath_func.hpp"
#include "core/sort_func.hpp"
#include "core/smallvec_type.hpp"
#include "date_type.h"

/** Flags of the sort list. */
enum SortListFlags {
	VL_NONE       = 0,      ///< no sort
	VL_DESC       = 1 << 0, ///< sort descending or ascending
	VL_RESORT     = 1 << 1, ///< instruct the code to resort the list in the next loop
	VL_REBUILD    = 1 << 2, ///< rebuild the sort list
	VL_FIRST_SORT = 1 << 3, ///< sort with quick sort first
	VL_FILTER     = 1 << 4, ///< filter disabled/enabled
	VL_END        = 1 << 5,
};
DECLARE_ENUM_AS_BIT_SET(SortListFlags)

/** Data structure describing how to show the list (what sort direction and criteria). */
struct Listing {
	bool order;    ///< Ascending/descending
	byte criteria; ///< Sorting criteria
};
/** Data structure describing what to show in the list (filter criteria). */
struct Filtering {
	bool state;    ///< Filter on/off
	byte criteria; ///< Filtering criteria
};

/**
 * List template of 'things' \p T to sort in a GUI.
 * @tparam T Type of data stored in the list to represent each item.
 * @tparam F Type of data fed as additional value to the filter function. @see FilterFunction
 */
template <typename T, typename F = const char*>
class GUIList : public std::vector<T> {
public:
	typedef bool SortFunction(const T&, const T&);  ///< Signature of sort function.
	typedef bool CDECL FilterFunction(const T*, F); ///< Signature of filter function.

protected:
	SortFunction * const *sort_func_list;     ///< the sort criteria functions
	FilterFunction * const *filter_func_list; ///< the filter criteria functions
	SortListFlags flags;                      ///< used to control sorting/resorting/etc.
	uint8 sort_type;                          ///< what criteria to sort on
	uint8 filter_type;                        ///< what criteria to filter on
	uint16 resort_timer;                      ///< resort list after a given amount of ticks if set

	/**
	 * Check if the list is sortable
	 *
	 * @return true if we can sort the list
	 */
	bool IsSortable() const
	{
		return std::vector<T>::size() >= 2;
	}

	/**
	 * Reset the resort timer
	 */
	void ResetResortTimer()
	{
		/* Resort every 10 days */
		this->resort_timer = DAY_TICKS * 10;
	}

public:
	GUIList() :
		sort_func_list(nullptr),
		filter_func_list(nullptr),
		flags(VL_FIRST_SORT),
		sort_type(0),
		filter_type(0),
		resort_timer(1)
	{};

	/**
	 * Get the sorttype of the list
	 *
	 * @return The current sorttype
	 */
	uint8 SortType() const
	{
		return this->sort_type;
	}

	/**
	 * Set the sorttype of the list
	 *
	 * @param n_type the new sort type
	 */
	void SetSortType(uint8 n_type)
	{
		if (this->sort_type != n_type) {
			SETBITS(this->flags, VL_RESORT | VL_FIRST_SORT);
			this->sort_type = n_type;
		}
	}

	/**
	 * Export current sort conditions
	 *
	 * @return the current sort conditions
	 */
	Listing GetListing() const
	{
		Listing l;
		l.order = (this->flags & VL_DESC) != 0;
		l.criteria = this->sort_type;

		return l;
	}

	/**
	 * Import sort conditions
	 *
	 * @param l The sort conditions we want to use
	 */
	void SetListing(Listing l)
	{
		if (l.order) {
			SETBITS(this->flags, VL_DESC);
		} else {
			CLRBITS(this->flags, VL_DESC);
		}
		this->sort_type = l.criteria;

		SETBITS(this->flags, VL_FIRST_SORT);
	}

	/**
	 * Get the filtertype of the list
	 *
	 * @return The current filtertype
	 */
	uint8 FilterType() const
	{
		return this->filter_type;
	}

	/**
	 * Set the filtertype of the list
	 *
	 * @param n_type the new filter type
	 */
	void SetFilterType(uint8 n_type)
	{
		if (this->filter_type != n_type) {
			this->filter_type = n_type;
		}
	}

	/**
	 * Export current filter conditions
	 *
	 * @return the current filter conditions
	 */
	Filtering GetFiltering() const
	{
		Filtering f;
		f.state = (this->flags & VL_FILTER) != 0;
		f.criteria = this->filter_type;

		return f;
	}

	/**
	 * Import filter conditions
	 *
	 * @param f The filter conditions we want to use
	 */
	void SetFiltering(Filtering f)
	{
		if (f.state) {
			SETBITS(this->flags, VL_FILTER);
		} else {
			CLRBITS(this->flags, VL_FILTER);
		}
		this->filter_type = f.criteria;
	}

	/**
	 * Check if a resort is needed next loop
	 *  If used the resort timer will decrease every call
	 *  till 0. If 0 reached the resort bit will be set and
	 *  the timer will be reset.
	 *
	 * @return true if resort bit is set for next loop
	 */
	bool NeedResort()
	{
		if (--this->resort_timer == 0) {
			SETBITS(this->flags, VL_RESORT);
			this->ResetResortTimer();
			return true;
		}
		return false;
	}

	/**
	 * Force a resort next Sort call
	 *  Reset the resort timer if used too.
	 */
	void ForceResort()
	{
		SETBITS(this->flags, VL_RESORT);
	}

	/**
	 * Check if the sort order is descending
	 *
	 * @return true if the sort order is descending
	 */
	bool IsDescSortOrder() const
	{
		return (this->flags & VL_DESC) != 0;
	}

	/**
	 * Toggle the sort order
	 *  Since that is the worst condition for the sort function
	 *  reverse the list here.
	 */
	void ToggleSortOrder()
	{
		this->flags ^= VL_DESC;

		if (this->IsSortable()) MemReverseT(std::vector<T>::data(), std::vector<T>::size());
	}

	/**
	 * Sort the list.
	 *  For the first sorting we use quick sort since it is
	 *  faster for irregular sorted data. After that we
	 *  use gsort.
	 *
	 * @param compare The function to compare two list items
	 * @return true if the list sequence has been altered
	 *
	 */
	bool Sort(SortFunction *compare)
	{
		/* Do not sort if the resort bit is not set */
		if (!(this->flags & VL_RESORT)) return false;

		CLRBITS(this->flags, VL_RESORT);

		this->ResetResortTimer();

		/* Do not sort when the list is not sortable */
		if (!this->IsSortable()) return false;

		const bool desc = (this->flags & VL_DESC) != 0;

		if (this->flags & VL_FIRST_SORT) {
			CLRBITS(this->flags, VL_FIRST_SORT);

			std::sort(std::vector<T>::begin(), std::vector<T>::end(), [&](const T &a, const T &b) { return desc ? compare(b, a) : compare(a, b); });
			return true;
		}

		std::sort(std::vector<T>::begin(), std::vector<T>::end(), [&](const T &a, const T &b) { return desc ? compare(b, a) : compare(a, b); });
		return true;
	}

	/**
	 * Hand the array of sort function pointers to the sort list
	 *
	 * @param n_funcs The pointer to the first sort func
	 */
	void SetSortFuncs(SortFunction * const *n_funcs)
	{
		this->sort_func_list = n_funcs;
	}

	/**
	 * Overload of #Sort(SortFunction *compare)
	 * Overloaded to reduce external code
	 *
	 * @return true if the list sequence has been altered
	 */
	bool Sort()
	{
		assert(this->sort_func_list != nullptr);
		return this->Sort(this->sort_func_list[this->sort_type]);
	}

	/**
	 * Check if the filter is enabled
	 *
	 * @return true if the filter is enabled
	 */
	bool IsFilterEnabled() const
	{
		return (this->flags & VL_FILTER) != 0;
	}

	/**
	 * Enable or disable the filter
	 *
	 * @param state If filtering should be enabled or disabled
	 */
	void SetFilterState(bool state)
	{
		if (state) {
			SETBITS(this->flags, VL_FILTER);
		} else {
			CLRBITS(this->flags, VL_FILTER);
		}
	}

	/**
	 * Filter the list.
	 *
	 * @param decide The function to decide about an item
	 * @param filter_data Additional data passed to the filter function
	 * @return true if the list has been altered by filtering
	 */
	bool Filter(FilterFunction *decide, F filter_data)
	{
		/* Do not filter if the filter bit is not set */
		if (!(this->flags & VL_FILTER)) return false;

		bool changed = false;
		for (auto it = std::vector<T>::begin(); it != std::vector<T>::end(); /* Nothing */) {
			if (!decide(&*it, filter_data)) {
				it = std::vector<T>::erase(it);
				changed = true;
			} else {
				it++;
			}
		}

		return changed;
	}

	/**
	 * Hand the array of filter function pointers to the sort list
	 *
	 * @param n_funcs The pointer to the first filter func
	 */
	void SetFilterFuncs(FilterFunction * const *n_funcs)
	{
		this->filter_func_list = n_funcs;
	}

	/**
	 * Filter the data with the currently selected filter.
	 *
	 * @param filter_data Additional data passed to the filter function.
	 * @return true if the list has been altered by filtering
	 */
	bool Filter(F filter_data)
	{
		if (this->filter_func_list == nullptr) return false;
		return this->Filter(this->filter_func_list[this->filter_type], filter_data);
	}

	/**
	 * Check if a rebuild is needed
	 * @return true if a rebuild is needed
	 */
	bool NeedRebuild() const
	{
		return (this->flags & VL_REBUILD) != 0;
	}

	/**
	 * Force that a rebuild is needed
	 */
	void ForceRebuild()
	{
		SETBITS(this->flags, VL_REBUILD);
	}

	/**
	 * Notify the sortlist that the rebuild is done
	 *
	 * @note This forces a resort
	 */
	void RebuildDone()
	{
		CLRBITS(this->flags, VL_REBUILD);
		SETBITS(this->flags, VL_RESORT | VL_FIRST_SORT);
	}
};

#endif /* SORTLIST_TYPE_H */
