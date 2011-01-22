/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_list.cpp Implementation of AIList. */

#include "../../stdafx.h"
#include "ai_list.hpp"
#include "../../debug.h"
#include "../../script/squirrel.hpp"

/**
 * Base class for any AIList sorter.
 */
class AIListSorter {
protected:
	AIList *list;

public:
	/**
	 * Virtual dtor, needed to mute warnings.
	 */
	virtual ~AIListSorter() { }

	/**
	 * Get the first item of the sorter.
	 */
	virtual int32 Begin() = 0;

	/**
	 * Stop iterating a sorter.
	 */
	virtual void End() = 0;

	/**
	 * Get the next item of the sorter.
	 */
	virtual int32 Next() = 0;

	/**
	 * See if the sorter has reached the end.
	 */
	virtual bool IsEnd() = 0;

	/**
	 * Callback from the list if an item gets removed.
	 */
	virtual void Remove(int item) = 0;
};

/**
 * Sort by value, ascending.
 */
class AIListSorterValueAscending : public AIListSorter {
private:
	AIList::AIListBucket::iterator bucket_iter;
	AIList::AIItemList *bucket_list;
	AIList::AIItemList::iterator bucket_list_iter;
	bool has_no_more_items;
	int32 item_next;

public:
	AIListSorterValueAscending(AIList *list)
	{
		this->list = list;
		this->End();
	}

	int32 Begin()
	{
		if (this->list->buckets.empty()) return 0;
		this->has_no_more_items = false;

		this->bucket_iter = this->list->buckets.begin();
		this->bucket_list = &(*this->bucket_iter).second;
		this->bucket_list_iter = this->bucket_list->begin();
		this->item_next = *this->bucket_list_iter;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void End()
	{
		this->bucket_list = NULL;
		this->has_no_more_items = true;
		this->item_next = 0;
	}

	void FindNext()
	{
		if (this->bucket_list == NULL) {
			this->has_no_more_items = true;
			return;
		}

		this->bucket_list_iter++;
		if (this->bucket_list_iter == this->bucket_list->end()) {
			this->bucket_iter++;
			if (this->bucket_iter == this->list->buckets.end()) {
				this->bucket_list = NULL;
				return;
			}
			this->bucket_list = &(*this->bucket_iter).second;
			this->bucket_list_iter = this->bucket_list->begin();
		}
		this->item_next = *this->bucket_list_iter;
	}

	int32 Next()
	{
		if (this->IsEnd()) return 0;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(int item)
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}

	bool IsEnd()
	{
		return this->list->buckets.empty() || this->has_no_more_items;
	}
};

/**
 * Sort by value, descending.
 */
class AIListSorterValueDescending : public AIListSorter {
private:
	AIList::AIListBucket::iterator bucket_iter;
	AIList::AIItemList *bucket_list;
	AIList::AIItemList::iterator bucket_list_iter;
	bool has_no_more_items;
	int32 item_next;

public:
	AIListSorterValueDescending(AIList *list)
	{
		this->list = list;
		this->End();
	}

	int32 Begin()
	{
		if (this->list->buckets.empty()) return 0;
		this->has_no_more_items = false;

		/* Go to the end of the bucket-list */
		this->bucket_iter = this->list->buckets.begin();
		for (size_t i = this->list->buckets.size(); i > 1; i--) this->bucket_iter++;
		this->bucket_list = &(*this->bucket_iter).second;

		/* Go to the end of the items in the bucket */
		this->bucket_list_iter = this->bucket_list->begin();
		for (size_t i = this->bucket_list->size(); i > 1; i--) this->bucket_list_iter++;
		this->item_next = *this->bucket_list_iter;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void End()
	{
		this->bucket_list = NULL;
		this->has_no_more_items = true;
		this->item_next = 0;
	}

	void FindNext()
	{
		if (this->bucket_list == NULL) {
			this->has_no_more_items = true;
			return;
		}

		if (this->bucket_list_iter == this->bucket_list->begin()) {
			if (this->bucket_iter == this->list->buckets.begin()) {
				this->bucket_list = NULL;
				return;
			}
			this->bucket_iter--;
			this->bucket_list = &(*this->bucket_iter).second;
			/* Go to the end of the items in the bucket */
			this->bucket_list_iter = this->bucket_list->begin();
			for (size_t i = this->bucket_list->size(); i > 1; i--) this->bucket_list_iter++;
		} else {
			this->bucket_list_iter--;
		}
		this->item_next = *this->bucket_list_iter;
	}

	int32 Next()
	{
		if (this->IsEnd()) return 0;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(int item)
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}

	bool IsEnd()
	{
		return this->list->buckets.empty() || this->has_no_more_items;
	}
};

/**
 * Sort by item, ascending.
 */
class AIListSorterItemAscending : public AIListSorter {
private:
	AIList::AIListMap::iterator item_iter;
	bool has_no_more_items;
	int32 item_next;

public:
	AIListSorterItemAscending(AIList *list)
	{
		this->list = list;
		this->End();
	}

	int32 Begin()
	{
		if (this->list->items.empty()) return 0;
		this->has_no_more_items = false;

		this->item_iter = this->list->items.begin();
		this->item_next = (*this->item_iter).first;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void End()
	{
		this->has_no_more_items = true;
	}

	void FindNext()
	{
		if (this->item_iter == this->list->items.end()) {
			this->has_no_more_items = true;
			return;
		}
		this->item_iter++;
		if (this->item_iter != this->list->items.end()) item_next = (*this->item_iter).first;
	}

	int32 Next()
	{
		if (this->IsEnd()) return 0;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(int item)
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}

	bool IsEnd()
	{
		return this->list->items.empty() || this->has_no_more_items;
	}
};

/**
 * Sort by item, descending.
 */
class AIListSorterItemDescending : public AIListSorter {
private:
	AIList::AIListMap::iterator item_iter;
	bool has_no_more_items;
	int32 item_next;

public:
	AIListSorterItemDescending(AIList *list)
	{
		this->list = list;
		this->End();
	}

	int32 Begin()
	{
		if (this->list->items.empty()) return 0;
		this->has_no_more_items = false;

		this->item_iter = this->list->items.begin();
		for (size_t i = this->list->items.size(); i > 1; i--) this->item_iter++;
		this->item_next = (*this->item_iter).first;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void End()
	{
		this->has_no_more_items = true;
	}

	void FindNext()
	{
		if (this->item_iter == this->list->items.end()) {
			this->has_no_more_items = true;
			return;
		}
		this->item_iter--;
		if (this->item_iter != this->list->items.end()) item_next = (*this->item_iter).first;
	}

	int32 Next()
	{
		if (this->IsEnd()) return 0;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(int item)
	{
		if (this->IsEnd()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}

	bool IsEnd()
	{
		return this->list->items.empty() || this->has_no_more_items;
	}
};



AIList::AIList()
{
	/* Default sorter */
	this->sorter         = new AIListSorterValueDescending(this);
	this->sorter_type    = SORT_BY_VALUE;
	this->sort_ascending = false;
	this->initialized    = false;
	this->modifications  = 0;
}

AIList::~AIList()
{
	delete this->sorter;
}

bool AIList::HasItem(int32 item)
{
	return this->items.count(item) == 1;
}

void AIList::Clear()
{
	this->modifications++;

	this->items.clear();
	this->buckets.clear();
	this->sorter->End();
}

void AIList::AddItem(int32 item, int32 value)
{
	this->modifications++;

	if (this->HasItem(item)) return;

	this->items[item] = 0;
	this->buckets[0].insert(item);

	this->SetValue(item, value);
}

void AIList::RemoveItem(int32 item)
{
	this->modifications++;

	if (!this->HasItem(item)) return;

	int32 value = this->GetValue(item);

	this->sorter->Remove(item);
	this->buckets[value].erase(item);
	if (this->buckets[value].empty()) this->buckets.erase(value);
	this->items.erase(item);
}

int32 AIList::Begin()
{
	this->initialized = true;
	return this->sorter->Begin();
}

int32 AIList::Next()
{
	if (this->initialized == false) {
		DEBUG(ai, 0, "Next() is invalid as Begin() is never called");
		return 0;
	}
	return this->sorter->Next();
}

bool AIList::IsEmpty()
{
	return this->items.empty();
}

bool AIList::IsEnd()
{
	if (this->initialized == false) {
		DEBUG(ai, 0, "IsEnd() is invalid as Begin() is never called");
		return true;
	}
	return this->sorter->IsEnd();
}

int32 AIList::Count()
{
	return (int32)this->items.size();
}

int32 AIList::GetValue(int32 item)
{
	if (!this->HasItem(item)) return 0;

	return this->items[item];
}

bool AIList::SetValue(int32 item, int32 value)
{
	this->modifications++;

	if (!this->HasItem(item)) return false;

	int32 value_old = this->GetValue(item);
	if (value_old == value) return true;

	this->sorter->Remove(item);
	this->buckets[value_old].erase(item);
	if (this->buckets[value_old].empty()) this->buckets.erase(value_old);
	this->items[item] = value;
	this->buckets[value].insert(item);

	return true;
}

void AIList::Sort(SorterType sorter, bool ascending)
{
	this->modifications++;

	if (sorter != SORT_BY_VALUE && sorter != SORT_BY_ITEM) return;
	if (sorter == this->sorter_type && ascending == this->sort_ascending) return;

	delete this->sorter;
	switch (sorter) {
		case SORT_BY_ITEM:
			if (ascending) {
				this->sorter = new AIListSorterItemAscending(this);
			} else {
				this->sorter = new AIListSorterItemDescending(this);
			}
			break;

		case SORT_BY_VALUE:
			if (ascending) {
				this->sorter = new AIListSorterValueAscending(this);
			} else {
				this->sorter = new AIListSorterValueDescending(this);
			}
			break;

		default:
			this->Sort(SORT_BY_ITEM, false);
			return;
	}
	this->sorter_type    = sorter;
	this->sort_ascending = ascending;
	this->initialized    = false;
}

void AIList::AddList(AIList *list)
{
	AIListMap *list_items = &list->items;
	for (AIListMap::iterator iter = list_items->begin(); iter != list_items->end(); iter++) {
		this->AddItem((*iter).first);
		this->SetValue((*iter).first, (*iter).second);
	}
}

void AIList::RemoveAboveValue(int32 value)
{
	this->modifications++;

	for (AIListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second > value) this->RemoveItem((*iter).first);
	}
}

void AIList::RemoveBelowValue(int32 value)
{
	this->modifications++;

	for (AIListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second < value) this->RemoveItem((*iter).first);
	}
}

void AIList::RemoveBetweenValue(int32 start, int32 end)
{
	this->modifications++;

	for (AIListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second > start && (*iter).second < end) this->RemoveItem((*iter).first);
	}
}

void AIList::RemoveValue(int32 value)
{
	this->modifications++;

	for (AIListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second == value) this->RemoveItem((*iter).first);
	}
}

void AIList::RemoveTop(int32 count)
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
			for (AIListBucket::iterator iter = this->buckets.begin(); iter != this->buckets.end(); iter = this->buckets.begin()) {
				AIItemList *items = &(*iter).second;
				size_t size = items->size();
				for (AIItemList::iterator iter = items->begin(); iter != items->end(); iter = items->begin()) {
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
			for (AIListMap::iterator iter = this->items.begin(); iter != this->items.end(); iter = this->items.begin()) {
				if (--count < 0) return;
				this->RemoveItem((*iter).first);
			}
			break;
	}
}

void AIList::RemoveBottom(int32 count)
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
			for (AIListBucket::reverse_iterator iter = this->buckets.rbegin(); iter != this->buckets.rend(); iter = this->buckets.rbegin()) {
				AIItemList *items = &(*iter).second;
				size_t size = items->size();
				for (AIItemList::reverse_iterator iter = items->rbegin(); iter != items->rend(); iter = items->rbegin()) {
					if (--count < 0) return;
					this->RemoveItem(*iter);
					/* When the last item is removed from the bucket, the bucket itself is removed.
					 * This means that the iterators can be invalid after a call to RemoveItem.
					 */
					if (--size == 0) break;
				}
			}

		case SORT_BY_ITEM:
			for (AIListMap::reverse_iterator iter = this->items.rbegin(); iter != this->items.rend(); iter = this->items.rbegin()) {
				if (--count < 0) return;
				this->RemoveItem((*iter).first);
			}
			break;
	}
}

void AIList::RemoveList(AIList *list)
{
	this->modifications++;

	AIListMap *list_items = &list->items;
	for (AIListMap::iterator iter = list_items->begin(); iter != list_items->end(); iter++) {
		this->RemoveItem((*iter).first);
	}
}

void AIList::KeepAboveValue(int32 value)
{
	this->modifications++;

	for (AIListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second <= value) this->RemoveItem((*iter).first);
	}
}

void AIList::KeepBelowValue(int32 value)
{
	this->modifications++;

	for (AIListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second >= value) this->RemoveItem((*iter).first);
	}
}

void AIList::KeepBetweenValue(int32 start, int32 end)
{
	this->modifications++;

	for (AIListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second <= start || (*iter).second >= end) this->RemoveItem((*iter).first);
	}
}

void AIList::KeepValue(int32 value)
{
	this->modifications++;

	for (AIListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second != value) this->RemoveItem((*iter).first);
	}
}

void AIList::KeepTop(int32 count)
{
	this->modifications++;

	this->RemoveBottom(this->Count() - count);
}

void AIList::KeepBottom(int32 count)
{
	this->modifications++;

	this->RemoveTop(this->Count() - count);
}

void AIList::KeepList(AIList *list)
{
	this->modifications++;

	AIList tmp;
	for (AIListMap::iterator iter = this->items.begin(); iter != this->items.end(); iter++) {
		tmp.AddItem((*iter).first);
		tmp.SetValue((*iter).first, (*iter).second);
	}

	tmp.RemoveList(list);
	this->RemoveList(&tmp);
}

SQInteger AIList::_get(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	if (!this->HasItem(idx)) return SQ_ERROR;

	sq_pushinteger(vm, this->GetValue(idx));
	return 1;
}

SQInteger AIList::_set(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;
	if (sq_gettype(vm, 3) != OT_INTEGER || sq_gettype(vm, 3) == OT_NULL) {
		return sq_throwerror(vm, _SC("you can only assign integers to this list"));
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

SQInteger AIList::_nexti(HSQUIRRELVM vm)
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

	int val = this->Next();
	if (this->IsEnd()) {
		sq_pushnull(vm);
		return 1;
	}

	sq_pushinteger(vm, val);
	return 1;
}

SQInteger AIList::Valuate(HSQUIRRELVM vm)
{
	this->modifications++;

	/* The first parameter is the instance of AIList. */
	int nparam = sq_gettop(vm) - 1;

	if (nparam < 1) {
		return sq_throwerror(vm, _SC("You need to give a least a Valuator as parameter to AIList::Valuate"));
	}

	/* Make sure the valuator function is really a function, and not any
	 * other type. It's parameter 2 for us, but for the user it's the
	 * first parameter they give. */
	SQObjectType valuator_type = sq_gettype(vm, 2);
	if (valuator_type != OT_CLOSURE && valuator_type != OT_NATIVECLOSURE) {
		return sq_throwerror(vm, _SC("parameter 1 has an invalid type (expected function)"));
	}

	/* Don't allow docommand from a Valuator, as we can't resume in
	 * mid C++-code. */
	bool backup_allow = AIObject::GetAllowDoCommand();
	AIObject::SetAllowDoCommand(false);

	/* Push the function to call */
	sq_push(vm, 2);

	for (AIListMap::iterator iter = this->items.begin(); iter != this->items.end(); iter++) {
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
			AIObject::SetAllowDoCommand(backup_allow);
			return SQ_ERROR;
		}

		/* Retreive the return value */
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

				AIObject::SetAllowDoCommand(backup_allow);
				return sq_throwerror(vm, _SC("return value of valuator is not valid (not integer/bool)"));
			}
		}

		/* Was something changed? */
		if (previous_modification_count != this->modifications) {
			/* See below for explanation. The extra pop is the return value. */
			sq_pop(vm, nparam + 4);

			AIObject::SetAllowDoCommand(backup_allow);
			return sq_throwerror(vm, _SC("modifying valuated list outside of valuator function"));
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
	 * 4. The AIList instance object. */
	sq_pop(vm, nparam + 3);

	AIObject::SetAllowDoCommand(backup_allow);
	return 0;
}
