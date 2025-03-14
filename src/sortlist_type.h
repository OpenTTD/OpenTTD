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
#include "timer/timer_game_tick.h"

/** Flags of the sort list. */
enum class SortListFlag : uint8_t {
	Desc, ///< sort descending or ascending
	Resort, ///< instruct the code to resort the list in the next loop
	Rebuild, ///< rebuild the sort list
	Filter, ///< filter disabled/enabled
};
using SortListFlags = EnumBitSet<SortListFlag, uint8_t>;

/** Data structure describing how to show the list (what sort direction and criteria). */
struct Listing {
	bool order;    ///< Ascending/descending
	uint8_t criteria; ///< Sorting criteria
};
/** Data structure describing what to show in the list (filter criteria). */
struct Filtering {
	bool state;    ///< Filter on/off
	uint8_t criteria; ///< Filtering criteria
};

/**
 * List template of 'things' \p T to sort in a GUI.
 * @tparam T Type of data stored in the list to represent each item.
 * @tparam P Tyoe of data passed as additional parameter to the sort function.
 * @tparam F Type of data fed as additional value to the filter function. @see FilterFunction
 */
template <typename T, typename P = std::nullptr_t, typename F = const char*>
class GUIList : public std::vector<T> {
public:
	using SortFunction = std::conditional_t<std::is_same_v<P, std::nullptr_t>, bool (const T&, const T&), bool (const T&, const T&, const P)>; ///< Signature of sort function.
	using FilterFunction = bool(const T*, F); ///< Signature of filter function.

protected:
	std::span<SortFunction * const> sort_func_list;     ///< the sort criteria functions
	std::span<FilterFunction * const> filter_func_list; ///< the filter criteria functions
	SortListFlags flags;                      ///< used to control sorting/resorting/etc.
	uint8_t sort_type;                          ///< what criteria to sort on
	uint8_t filter_type;                        ///< what criteria to filter on
	uint16_t resort_timer;                      ///< resort list after a given amount of ticks if set

	/* If sort parameters are used then params must be a reference, however if not then params cannot be a reference as
	 * it will not be able to reference anything. */
	using SortParameterReference = std::conditional_t<std::is_same_v<P, std::nullptr_t>, P, P&>;
	const SortParameterReference params;

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
		this->resort_timer = Ticks::DAY_TICKS * 10;
	}

public:
	/* If sort parameters are not used then we don't require a reference to the params. */
	template <typename T_ = T, typename P_ = P, typename _F = F, std::enable_if_t<std::is_same_v<P_, std::nullptr_t>>* = nullptr>
	GUIList() :
		sort_func_list({}),
		filter_func_list({}),
		flags({}),
		sort_type(0),
		filter_type(0),
		resort_timer(1),
		params(nullptr)
	{};

	/* If sort parameters are used then we require a reference to the params. */
	template <typename T_ = T, typename P_ = P, typename _F = F, std::enable_if_t<!std::is_same_v<P_, std::nullptr_t>>* = nullptr>
	GUIList(const P &params) :
		sort_func_list({}),
		filter_func_list({}),
		flags({}),
		sort_type(0),
		filter_type(0),
		resort_timer(1),
		params(params)
	{};

	/**
	 * Get the sorttype of the list
	 *
	 * @return The current sorttype
	 */
	uint8_t SortType() const
	{
		return this->sort_type;
	}

	/**
	 * Set the sorttype of the list
	 *
	 * @param n_type the new sort type
	 */
	void SetSortType(uint8_t n_type)
	{
		assert(n_type < std::size(this->sort_func_list));
		if (this->sort_type != n_type) {
			this->flags.Set(SortListFlag::Resort);
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
		l.order = this->flags.Test(SortListFlag::Desc);
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
			this->flags.Set(SortListFlag::Desc);
		} else {
			this->flags.Reset(SortListFlag::Desc);
		}
		this->sort_type = l.criteria;
	}

	/**
	 * Get the filtertype of the list
	 *
	 * @return The current filtertype
	 */
	uint8_t FilterType() const
	{
		return this->filter_type;
	}

	/**
	 * Set the filtertype of the list
	 *
	 * @param n_type the new filter type
	 */
	void SetFilterType(uint8_t n_type)
	{
		assert(n_type < std::size(this->filter_func_list));
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
		f.state = this->flags.Test(SortListFlag::Filter);
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
			this->flags.Set(SortListFlag::Filter);
		} else {
			this->flags.Reset(SortListFlag::Filter);
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
			this->flags.Set(SortListFlag::Resort);
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
		this->flags.Set(SortListFlag::Resort);
	}

	/**
	 * Check if the sort order is descending
	 *
	 * @return true if the sort order is descending
	 */
	bool IsDescSortOrder() const
	{
		return this->flags.Test(SortListFlag::Desc);
	}

	/**
	 * Toggle the sort order
	 *  Since that is the worst condition for the sort function
	 *  reverse the list here.
	 */
	void ToggleSortOrder()
	{
		this->flags.Flip(SortListFlag::Desc);

		if (this->IsSortable()) std::reverse(std::vector<T>::begin(), std::vector<T>::end());
	}

	/**
	 * Sort the list.
	 * @param compare The function to compare two list items
	 * @return true if the list sequence has been altered
	 *
	 */
	template <typename Comp>
	bool Sort(Comp compare)
	{
		/* Do not sort if the resort bit is not set */
		if (!this->flags.Test(SortListFlag::Resort)) return false;

		this->flags.Reset(SortListFlag::Resort);

		this->ResetResortTimer();

		/* Do not sort when the list is not sortable */
		if (!this->IsSortable()) return false;

		const bool desc = this->flags.Test(SortListFlag::Desc);

		if constexpr (std::is_same_v<P, std::nullptr_t>) {
			std::sort(std::vector<T>::begin(), std::vector<T>::end(), [&](const T &a, const T &b) { return desc ? compare(b, a) : compare(a, b); });
		} else {
			std::sort(std::vector<T>::begin(), std::vector<T>::end(), [&](const T &a, const T &b) { return desc ? compare(b, a, params) : compare(a, b, params); });
		}
		return true;
	}

	/**
	 * Hand the sort function pointers to the GUIList.
	 *
	 * @param n_funcs Span covering the sort function pointers.
	 */
	void SetSortFuncs(std::span<SortFunction * const> n_funcs)
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
		if (this->sort_func_list.empty()) return false;
		assert(this->sort_type < this->sort_func_list.size());
		return this->Sort(this->sort_func_list[this->sort_type]);
	}

	/**
	 * Check if the filter is enabled
	 *
	 * @return true if the filter is enabled
	 */
	bool IsFilterEnabled() const
	{
		return this->flags.Test(SortListFlag::Filter);
	}

	/**
	 * Enable or disable the filter
	 *
	 * @param state If filtering should be enabled or disabled
	 */
	void SetFilterState(bool state)
	{
		if (state) {
			this->flags.Set(SortListFlag::Filter);
		} else {
			this->flags.Reset(SortListFlag::Filter);
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
		if (!this->flags.Test(SortListFlag::Filter)) return false;

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
	 * Hand the filter function pointers to the GUIList.
	 *
	 * @param n_funcs Span covering the filter function pointers.
	 */
	void SetFilterFuncs(std::span<FilterFunction * const> n_funcs)
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
		if (this->filter_func_list.empty()) return false;
		assert(this->filter_type < this->filter_func_list.size());
		return this->Filter(this->filter_func_list[this->filter_type], filter_data);
	}

	/**
	 * Check if a rebuild is needed
	 * @return true if a rebuild is needed
	 */
	bool NeedRebuild() const
	{
		return this->flags.Test(SortListFlag::Rebuild);
	}

	/**
	 * Force that a rebuild is needed
	 */
	void ForceRebuild()
	{
		this->flags.Set(SortListFlag::Rebuild);
	}

	/**
	 * Notify the sortlist that the rebuild is done
	 *
	 * @note This forces a resort
	 */
	void RebuildDone()
	{
		this->flags.Reset(SortListFlag::Rebuild);
		this->flags.Set(SortListFlag::Resort);
	}
};

#endif /* SORTLIST_TYPE_H */
