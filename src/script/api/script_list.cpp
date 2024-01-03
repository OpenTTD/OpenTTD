/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_list.cpp Implementation of ScriptList. */

#include "../../stdafx.h"
#include "script_list.hpp"
#include "script_controller.hpp"
#include "../../debug.h"
#include "../../core/backup_type.hpp"
#include "../../script/squirrel.hpp"
#include <../squirrel/sqvm.h>

#include "../../safeguards.h"

/**
 * Base class for any ScriptList sorter.
 */
class ScriptListSorter {
protected:
	ScriptList *list;       ///< The list that's being sorted.
	bool has_no_more_items; ///< Whether we have more items to iterate over.
	SQInteger item_next;    ///< The next item we will show.

public:
	/**
	 * Virtual dtor, needed to mute warnings.
	 */
	virtual ~ScriptListSorter() = default;

	/**
	 * Get the first item of the sorter.
	 */
	virtual SQInteger Begin() = 0;

	/**
	 * Stop iterating a sorter.
	 */
	virtual void End() = 0;

	/**
	 * Get the next item of the sorter.
	 */
	virtual SQInteger Next() = 0;

	/**
	 * See if the sorter has reached the end.
	 */
	bool IsEnd()
	{
		return this->list->buckets.empty() || this->has_no_more_items;
	}

	/**
	 * Callback from the list if an item gets removed.
	 */
	virtual void Remove(SQInteger item) = 0;

	/**
	 * Attach the sorter to a new list. This assumes the content of the old list has been moved to
	 * the new list, too, so that we don't have to invalidate any iterators. Note that std::swap
	 * doesn't invalidate iterators on lists and maps, so that should be safe.
	 * @param target New list to attach to.
	 */
	virtual void Retarget(ScriptList *new_list)
	{
		this->list = new_list;
	}
};

/**
 * Sort by value, ascending.
 */
class ScriptListSorterValueAscending : public ScriptListSorter {
private:
	ScriptList::ScriptListBucket::iterator bucket_iter;    ///< The iterator over the list to find the buckets.
	ScriptList::ScriptItemList *bucket_list;               ///< The current bucket list we're iterator over.
	ScriptList::ScriptItemList::iterator bucket_list_iter; ///< The iterator over the bucket list.

public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterValueAscending(ScriptList *list)
	{
		this->list = list;
		this->End();
	}

	SQInteger Begin() override
	{
		if (this->list->buckets.empty()) return 0;
		this->has_no_more_items = false;

		this->bucket_iter = this->list->buckets.begin();
		this->bucket_list = &(*this->bucket_iter).second;
		this->bucket_list_iter = this->bucket_list->begin();
		this->item_next = *this->bucket_list_iter;

		SQInteger item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void End() override
	{
		this->bucket_list = nullptr;
		this->has_no_more_items = true;
		this->item_next = 0;
	}

	/**
	 * Find the next item, and store that information.
	 */
	void FindNext()
	{
		if (this->bucket_list == nullptr) {
			this->has_no_more_items = true;
			return;
		}

		this->bucket_list_iter++;
		if (this->bucket_list_iter == this->bucket_list->end()) {
			this->bucket_iter++;
			if (this->bucket_iter == this->list->buckets.end()) {
				this->bucket_list = nullptr;
				return;
			}
			this->bucket_list = &(*this->bucket_iter).second;
			this->bucket_list_iter = this->bucket_list->begin();
		}
		this->item_next = *this->bucket_list_iter;
	}

	SQInteger Next() override
	{
		if (this->IsEnd()) return 0;

		SQInteger item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(SQInteger item) override
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}
};

/**
 * Sort by value, descending.
 */
class ScriptListSorterValueDescending : public ScriptListSorter {
private:
	/* Note: We cannot use reverse_iterator.
	 *       The iterators must only be invalidated when the element they are pointing to is removed.
	 *       This only holds for forward iterators. */
	ScriptList::ScriptListBucket::iterator bucket_iter;    ///< The iterator over the list to find the buckets.
	ScriptList::ScriptItemList *bucket_list;               ///< The current bucket list we're iterator over.
	ScriptList::ScriptItemList::iterator bucket_list_iter; ///< The iterator over the bucket list.

public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterValueDescending(ScriptList *list)
	{
		this->list = list;
		this->End();
	}

	SQInteger Begin() override
	{
		if (this->list->buckets.empty()) return 0;
		this->has_no_more_items = false;

		/* Go to the end of the bucket-list */
		this->bucket_iter = this->list->buckets.end();
		--this->bucket_iter;
		this->bucket_list = &(*this->bucket_iter).second;

		/* Go to the end of the items in the bucket */
		this->bucket_list_iter = this->bucket_list->end();
		--this->bucket_list_iter;
		this->item_next = *this->bucket_list_iter;

		SQInteger item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void End() override
	{
		this->bucket_list = nullptr;
		this->has_no_more_items = true;
		this->item_next = 0;
	}

	/**
	 * Find the next item, and store that information.
	 */
	void FindNext()
	{
		if (this->bucket_list == nullptr) {
			this->has_no_more_items = true;
			return;
		}

		if (this->bucket_list_iter == this->bucket_list->begin()) {
			if (this->bucket_iter == this->list->buckets.begin()) {
				this->bucket_list = nullptr;
				return;
			}
			this->bucket_iter--;
			this->bucket_list = &(*this->bucket_iter).second;
			/* Go to the end of the items in the bucket */
			this->bucket_list_iter = this->bucket_list->end();
			--this->bucket_list_iter;
		} else {
			this->bucket_list_iter--;
		}
		this->item_next = *this->bucket_list_iter;
	}

	SQInteger Next() override
	{
		if (this->IsEnd()) return 0;

		SQInteger item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(SQInteger item) override
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}
};

/**
 * Sort by item, ascending.
 */
class ScriptListSorterItemAscending : public ScriptListSorter {
private:
	ScriptList::ScriptListMap::iterator item_iter; ///< The iterator over the items in the map.

public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterItemAscending(ScriptList *list)
	{
		this->list = list;
		this->End();
	}

	SQInteger Begin() override
	{
		if (this->list->items.empty()) return 0;
		this->has_no_more_items = false;

		this->item_iter = this->list->items.begin();
		this->item_next = (*this->item_iter).first;

		SQInteger item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void End() override
	{
		this->has_no_more_items = true;
	}

	/**
	 * Find the next item, and store that information.
	 */
	void FindNext()
	{
		if (this->item_iter == this->list->items.end()) {
			this->has_no_more_items = true;
			return;
		}
		this->item_iter++;
		if (this->item_iter != this->list->items.end()) item_next = (*this->item_iter).first;
	}

	SQInteger Next() override
	{
		if (this->IsEnd()) return 0;

		SQInteger item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(SQInteger item) override
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}
};

/**
 * Sort by item, descending.
 */
class ScriptListSorterItemDescending : public ScriptListSorter {
private:
	/* Note: We cannot use reverse_iterator.
	 *       The iterators must only be invalidated when the element they are pointing to is removed.
	 *       This only holds for forward iterators. */
	ScriptList::ScriptListMap::iterator item_iter; ///< The iterator over the items in the map.

public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterItemDescending(ScriptList *list)
	{
		this->list = list;
		this->End();
	}

	SQInteger Begin() override
	{
		if (this->list->items.empty()) return 0;
		this->has_no_more_items = false;

		this->item_iter = this->list->items.end();
		--this->item_iter;
		this->item_next = (*this->item_iter).first;

		SQInteger item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void End() override
	{
		this->has_no_more_items = true;
	}

	/**
	 * Find the next item, and store that information.
	 */
	void FindNext()
	{
		if (this->item_iter == this->list->items.end()) {
			this->has_no_more_items = true;
			return;
		}
		if (this->item_iter == this->list->items.begin()) {
			/* Use 'end' as marker for 'beyond begin' */
			this->item_iter = this->list->items.end();
		} else {
			this->item_iter--;
		}
		if (this->item_iter != this->list->items.end()) item_next = (*this->item_iter).first;
	}

	SQInteger Next() override
	{
		if (this->IsEnd()) return 0;

		SQInteger item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(SQInteger item) override
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}
};



ScriptList::ScriptList()
{
	/* Default sorter */
	this->sorter         = new ScriptListSorterValueDescending(this);
	this->sorter_type    = SORT_BY_VALUE;
	this->sort_ascending = false;
	this->initialized    = false;
	this->modifications  = 0;
}

ScriptList::~ScriptList()
{
	delete this->sorter;
}

bool ScriptList::HasItem(SQInteger item)
{
	return this->items.count(item) == 1;
}

void ScriptList::Clear()
{
	this->modifications++;

	this->items.clear();
	this->buckets.clear();
	this->sorter->End();
}

void ScriptList::AddItem(SQInteger item, SQInteger value)
{
	this->modifications++;

	if (this->HasItem(item)) return;

	this->items[item] = value;
	this->buckets[value].insert(item);
}

void ScriptList::RemoveItem(SQInteger item)
{
	this->modifications++;

	ScriptListMap::iterator item_iter = this->items.find(item);
	if (item_iter == this->items.end()) return;

	SQInteger value = item_iter->second;

	this->sorter->Remove(item);
	ScriptListBucket::iterator bucket_iter = this->buckets.find(value);
	assert(bucket_iter != this->buckets.end());
	bucket_iter->second.erase(item);
	if (bucket_iter->second.empty()) this->buckets.erase(bucket_iter);
	this->items.erase(item_iter);
}

SQInteger ScriptList::Begin()
{
	this->initialized = true;
	return this->sorter->Begin();
}

SQInteger ScriptList::Next()
{
	if (!this->initialized) {
		Debug(script, 0, "Next() is invalid as Begin() is never called");
		return 0;
	}
	return this->sorter->Next();
}

bool ScriptList::IsEmpty()
{
	return this->items.empty();
}

bool ScriptList::IsEnd()
{
	if (!this->initialized) {
		Debug(script, 0, "IsEnd() is invalid as Begin() is never called");
		return true;
	}
	return this->sorter->IsEnd();
}

SQInteger ScriptList::Count()
{
	return this->items.size();
}

SQInteger ScriptList::GetValue(SQInteger item)
{
	ScriptListMap::const_iterator item_iter = this->items.find(item);
	return item_iter == this->items.end() ? 0 : item_iter->second;
}

bool ScriptList::SetValue(SQInteger item, SQInteger value)
{
	this->modifications++;

	ScriptListMap::iterator item_iter = this->items.find(item);
	if (item_iter == this->items.end()) return false;

	SQInteger value_old = item_iter->second;
	if (value_old == value) return true;

	this->sorter->Remove(item);
	ScriptListBucket::iterator bucket_iter = this->buckets.find(value_old);
	assert(bucket_iter != this->buckets.end());
	bucket_iter->second.erase(item);
	if (bucket_iter->second.empty()) this->buckets.erase(bucket_iter);
	item_iter->second = value;
	this->buckets[value].insert(item);

	return true;
}

void ScriptList::Sort(SorterType sorter, bool ascending)
{
	this->modifications++;

	if (sorter != SORT_BY_VALUE && sorter != SORT_BY_ITEM) return;
	if (sorter == this->sorter_type && ascending == this->sort_ascending) return;

	delete this->sorter;
	switch (sorter) {
		case SORT_BY_ITEM:
			if (ascending) {
				this->sorter = new ScriptListSorterItemAscending(this);
			} else {
				this->sorter = new ScriptListSorterItemDescending(this);
			}
			break;

		case SORT_BY_VALUE:
			if (ascending) {
				this->sorter = new ScriptListSorterValueAscending(this);
			} else {
				this->sorter = new ScriptListSorterValueDescending(this);
			}
			break;

		default: NOT_REACHED();
	}
	this->sorter_type    = sorter;
	this->sort_ascending = ascending;
	this->initialized    = false;
}

void ScriptList::AddList(ScriptList *list)
{
	if (list == this) return;

	if (this->IsEmpty()) {
		/* If this is empty, we can just take the items of the other list as is. */
		this->items = list->items;
		this->buckets = list->buckets;
		this->modifications++;
	} else {
		ScriptListMap *list_items = &list->items;
		for (auto &it : *list_items) {
			this->AddItem(it.first);
			this->SetValue(it.first, it.second);
		}
	}
}

void ScriptList::SwapList(ScriptList *list)
{
	if (list == this) return;

	this->items.swap(list->items);
	this->buckets.swap(list->buckets);
	Swap(this->sorter, list->sorter);
	Swap(this->sorter_type, list->sorter_type);
	Swap(this->sort_ascending, list->sort_ascending);
	Swap(this->initialized, list->initialized);
	Swap(this->modifications, list->modifications);
	this->sorter->Retarget(this);
	list->sorter->Retarget(list);
}

void ScriptList::RemoveAboveValue(SQInteger value)
{
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second > value) this->RemoveItem((*iter).first);
	}
}

void ScriptList::RemoveBelowValue(SQInteger value)
{
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second < value) this->RemoveItem((*iter).first);
	}
}

void ScriptList::RemoveBetweenValue(SQInteger start, SQInteger end)
{
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second > start && (*iter).second < end) this->RemoveItem((*iter).first);
	}
}

void ScriptList::RemoveValue(SQInteger value)
{
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second == value) this->RemoveItem((*iter).first);
	}
}

void ScriptList::RemoveTop(SQInteger count)
{
	this->modifications++;

	if (!this->sort_ascending) {
		this->Sort(this->sorter_type, !this->sort_ascending);
		this->RemoveBottom(count);
		this->Sort(this->sorter_type, !this->sort_ascending);
		return;
	}

	switch (this->sorter_type) {
		default: NOT_REACHED();
		case SORT_BY_VALUE:
			for (ScriptListBucket::iterator iter = this->buckets.begin(); iter != this->buckets.end(); iter = this->buckets.begin()) {
				ScriptItemList *items = &(*iter).second;
				size_t size = items->size();
				for (ScriptItemList::iterator iter = items->begin(); iter != items->end(); iter = items->begin()) {
					if (--count < 0) return;
					this->RemoveItem(*iter);
					/* When the last item is removed from the bucket, the bucket itself is removed.
					 * This means that the iterators can be invalid after a call to RemoveItem.
					 */
					if (--size == 0) break;
				}
			}
			break;

		case SORT_BY_ITEM:
			for (ScriptListMap::iterator iter = this->items.begin(); iter != this->items.end(); iter = this->items.begin()) {
				if (--count < 0) return;
				this->RemoveItem((*iter).first);
			}
			break;
	}
}

void ScriptList::RemoveBottom(SQInteger count)
{
	this->modifications++;

	if (!this->sort_ascending) {
		this->Sort(this->sorter_type, !this->sort_ascending);
		this->RemoveTop(count);
		this->Sort(this->sorter_type, !this->sort_ascending);
		return;
	}

	switch (this->sorter_type) {
		default: NOT_REACHED();
		case SORT_BY_VALUE:
			for (ScriptListBucket::reverse_iterator iter = this->buckets.rbegin(); iter != this->buckets.rend(); iter = this->buckets.rbegin()) {
				ScriptItemList *items = &(*iter).second;
				size_t size = items->size();
				for (ScriptItemList::reverse_iterator iter = items->rbegin(); iter != items->rend(); iter = items->rbegin()) {
					if (--count < 0) return;
					this->RemoveItem(*iter);
					/* When the last item is removed from the bucket, the bucket itself is removed.
					 * This means that the iterators can be invalid after a call to RemoveItem.
					 */
					if (--size == 0) break;
				}
			}
			break;

		case SORT_BY_ITEM:
			for (ScriptListMap::reverse_iterator iter = this->items.rbegin(); iter != this->items.rend(); iter = this->items.rbegin()) {
				if (--count < 0) return;
				this->RemoveItem((*iter).first);
			}
			break;
	}
}

void ScriptList::RemoveList(ScriptList *list)
{
	this->modifications++;

	if (list == this) {
		Clear();
	} else {
		ScriptListMap *list_items = &list->items;
		for (ScriptListMap::iterator iter = list_items->begin(); iter != list_items->end(); iter++) {
			this->RemoveItem((*iter).first);
		}
	}
}

void ScriptList::KeepAboveValue(SQInteger value)
{
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second <= value) this->RemoveItem((*iter).first);
	}
}

void ScriptList::KeepBelowValue(SQInteger value)
{
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second >= value) this->RemoveItem((*iter).first);
	}
}

void ScriptList::KeepBetweenValue(SQInteger start, SQInteger end)
{
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second <= start || (*iter).second >= end) this->RemoveItem((*iter).first);
	}
}

void ScriptList::KeepValue(SQInteger value)
{
	this->modifications++;

	for (ScriptListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second != value) this->RemoveItem((*iter).first);
	}
}

void ScriptList::KeepTop(SQInteger count)
{
	this->modifications++;

	this->RemoveBottom(this->Count() - count);
}

void ScriptList::KeepBottom(SQInteger count)
{
	this->modifications++;

	this->RemoveTop(this->Count() - count);
}

void ScriptList::KeepList(ScriptList *list)
{
	if (list == this) return;

	this->modifications++;

	ScriptList tmp;
	tmp.AddList(this);
	tmp.RemoveList(list);
	this->RemoveList(&tmp);
}

SQInteger ScriptList::_get(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	ScriptListMap::const_iterator item_iter = this->items.find(idx);
	if (item_iter == this->items.end()) return SQ_ERROR;

	sq_pushinteger(vm, item_iter->second);
	return 1;
}

SQInteger ScriptList::_set(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;
	if (sq_gettype(vm, 3) != OT_INTEGER && sq_gettype(vm, 3) != OT_NULL) {
		return sq_throwerror(vm, "you can only assign integers to this list");
	}

	SQInteger idx, val;
	sq_getinteger(vm, 2, &idx);
	if (sq_gettype(vm, 3) == OT_NULL) {
		this->RemoveItem(idx);
		return 0;
	}

	sq_getinteger(vm, 3, &val);
	if (!this->HasItem(idx)) {
		this->AddItem(idx, val);
		return 0;
	}

	this->SetValue(idx, val);
	return 0;
}

SQInteger ScriptList::_nexti(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) == OT_NULL) {
		if (this->IsEmpty()) {
			sq_pushnull(vm);
			return 1;
		}
		sq_pushinteger(vm, this->Begin());
		return 1;
	}

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	SQInteger val = this->Next();
	if (this->IsEnd()) {
		sq_pushnull(vm);
		return 1;
	}

	sq_pushinteger(vm, val);
	return 1;
}

SQInteger ScriptList::Valuate(HSQUIRRELVM vm)
{
	this->modifications++;

	/* The first parameter is the instance of ScriptList. */
	int nparam = sq_gettop(vm) - 1;

	if (nparam < 1) {
		return sq_throwerror(vm, "You need to give at least a Valuator as parameter to ScriptList::Valuate");
	}

	/* Make sure the valuator function is really a function, and not any
	 * other type. It's parameter 2 for us, but for the user it's the
	 * first parameter they give. */
	SQObjectType valuator_type = sq_gettype(vm, 2);
	if (valuator_type != OT_CLOSURE && valuator_type != OT_NATIVECLOSURE) {
		return sq_throwerror(vm, "parameter 1 has an invalid type (expected function)");
	}

	/* Don't allow docommand from a Valuator, as we can't resume in
	 * mid C++-code. */
	bool backup_allow = ScriptObject::GetAllowDoCommand();
	ScriptObject::SetAllowDoCommand(false);

	/* Limit the total number of ops that can be consumed by a valuate operation */
	SQInteger new_ops_error_threshold = vm->_ops_till_suspend_error_threshold;
	if (vm->_ops_till_suspend_error_threshold == INT64_MIN) {
		new_ops_error_threshold = vm->_ops_till_suspend - MAX_VALUATE_OPS;
		vm->_ops_till_suspend_error_label = "valuator function";
	}
	AutoRestoreBackup ops_error_threshold_backup(vm->_ops_till_suspend_error_threshold, new_ops_error_threshold);

	/* Push the function to call */
	sq_push(vm, 2);

	for (ScriptListMap::iterator iter = this->items.begin(); iter != this->items.end(); iter++) {
		/* Check for changing of items. */
		int previous_modification_count = this->modifications;

		/* Push the root table as instance object, this is what squirrel does for meta-functions. */
		sq_pushroottable(vm);
		/* Push all arguments for the valuator function. */
		sq_pushinteger(vm, (*iter).first);
		for (int i = 0; i < nparam - 1; i++) {
			sq_push(vm, i + 3);
		}

		/* Call the function. Squirrel pops all parameters and pushes the return value. */
		if (SQ_FAILED(sq_call(vm, nparam + 1, SQTrue, SQTrue))) {
			ScriptObject::SetAllowDoCommand(backup_allow);
			return SQ_ERROR;
		}

		/* Retrieve the return value */
		SQInteger value;
		switch (sq_gettype(vm, -1)) {
			case OT_INTEGER: {
				sq_getinteger(vm, -1, &value);
				break;
			}

			case OT_BOOL: {
				SQBool v;
				sq_getbool(vm, -1, &v);
				value = v ? 1 : 0;
				break;
			}

			default: {
				/* See below for explanation. The extra pop is the return value. */
				sq_pop(vm, nparam + 4);

				ScriptObject::SetAllowDoCommand(backup_allow);
				return sq_throwerror(vm, "return value of valuator is not valid (not integer/bool)");
			}
		}

		/* Kill the script when the valuator call takes way too long.
		 * Triggered by nesting valuators, which then take billions of iterations. */
		if (ScriptController::GetOpsTillSuspend() < -1000000) {
			/* See below for explanation. The extra pop is the return value. */
			sq_pop(vm, nparam + 4);

			ScriptObject::SetAllowDoCommand(backup_allow);
			return sq_throwerror(vm, "excessive CPU usage in valuator function");
		}

		/* Was something changed? */
		if (previous_modification_count != this->modifications) {
			/* See below for explanation. The extra pop is the return value. */
			sq_pop(vm, nparam + 4);

			ScriptObject::SetAllowDoCommand(backup_allow);
			return sq_throwerror(vm, "modifying valuated list outside of valuator function");
		}

		this->SetValue((*iter).first, value);

		/* Pop the return value. */
		sq_poptop(vm);

		Squirrel::DecreaseOps(vm, 5);
	}
	/* Pop from the squirrel stack:
	 * 1. The root stable (as instance object).
	 * 2. The valuator function.
	 * 3. The parameters given to this function.
	 * 4. The ScriptList instance object. */
	sq_pop(vm, nparam + 3);

	ScriptObject::SetAllowDoCommand(backup_allow);
	return 0;
}
