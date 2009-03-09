/* $Id$ */

/** @file ai_abstractlist.cpp Implementation of AIAbstractList. */

#include <squirrel.h>
#include "ai_abstractlist.hpp"
#include "../../debug.h"
#include "../../core/alloc_func.hpp"
#include "../../script/squirrel.hpp"

/**
 * Base class for any AIAbstractList sorter.
 */
class AIAbstractListSorter {
protected:
	AIAbstractList *list;

public:
	/**
	 * Virtual dtor, needed to mute warnings.
	 */
	virtual ~AIAbstractListSorter() { }

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
	 * See if there is a next item of the sorter.
	 */
	virtual bool HasNext() = 0;

	/**
	 * Callback from the list if an item gets removed.
	 */
	virtual void Remove(int item) = 0;
};

/**
 * Sort by value, ascending.
 */
class AIAbstractListSorterValueAscending : public AIAbstractListSorter {
private:
	AIAbstractList::AIAbstractListBucket::iterator bucket_iter;
	AIAbstractList::AIItemList *bucket_list;
	AIAbstractList::AIItemList::iterator bucket_list_iter;
	bool has_no_more_items;
	int32 item_next;

public:
	AIAbstractListSorterValueAscending(AIAbstractList *list)
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
		if (!this->HasNext()) return 0;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(int item)
	{
		if (!this->HasNext()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}

	bool HasNext()
	{
		return !(this->list->buckets.empty() || this->has_no_more_items);
	}
};

/**
 * Sort by value, descending.
 */
class AIAbstractListSorterValueDescending : public AIAbstractListSorter {
private:
	AIAbstractList::AIAbstractListBucket::iterator bucket_iter;
	AIAbstractList::AIItemList *bucket_list;
	AIAbstractList::AIItemList::iterator bucket_list_iter;
	bool has_no_more_items;
	int32 item_next;

public:
	AIAbstractListSorterValueDescending(AIAbstractList *list)
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

	void End() {
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
		if (!this->HasNext()) return 0;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(int item)
	{
		if (!this->HasNext()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}

	bool HasNext()
	{
		return !(this->list->buckets.empty() || this->has_no_more_items);
	}
};

/**
 * Sort by item, ascending.
 */
class AIAbstractListSorterItemAscending : public AIAbstractListSorter {
private:
	AIAbstractList::AIAbstractListMap::iterator item_iter;
	bool has_no_more_items;
	int32 item_next;

public:
	AIAbstractListSorterItemAscending(AIAbstractList *list)
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
		if (!this->HasNext()) return 0;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(int item) {
		if (!this->HasNext()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}

	bool HasNext()
	{
		return !(this->list->items.empty() || this->has_no_more_items);
	}
};

/**
 * Sort by item, descending.
 */
class AIAbstractListSorterItemDescending : public AIAbstractListSorter {
private:
	AIAbstractList::AIAbstractListMap::iterator item_iter;
	bool has_no_more_items;
	int32 item_next;

public:
	AIAbstractListSorterItemDescending(AIAbstractList *list)
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
		if (!this->HasNext()) return 0;

		int32 item_current = this->item_next;
		FindNext();
		return item_current;
	}

	void Remove(int item)
	{
		if (!this->HasNext()) return;

		/* If we remove the 'next' item, skip to the next */
		if (item == this->item_next) {
			FindNext();
			return;
		}
	}

	bool HasNext()
	{
		return !(this->list->items.empty() || this->has_no_more_items);
	}
};



AIAbstractList::AIAbstractList()
{
	/* Default sorter */
	this->sorter         = new AIAbstractListSorterValueDescending(this);
	this->sorter_type    = SORT_BY_VALUE;
	this->sort_ascending = false;
	this->initialized    = false;
}

AIAbstractList::~AIAbstractList()
{
	delete this->sorter;
}

bool AIAbstractList::HasItem(int32 item)
{
	return this->items.count(item) == 1;
}

void AIAbstractList::Clear()
{
	this->items.clear();
	this->buckets.clear();
	this->sorter->End();
}

void AIAbstractList::AddItem(int32 item)
{
	if (this->HasItem(item)) return;

	this->items[item] = 0;
	this->buckets[0].insert(item);
}

void AIAbstractList::RemoveItem(int32 item)
{
	if (!this->HasItem(item)) return;

	int32 value = this->GetValue(item);

	this->sorter->Remove(item);
	this->buckets[value].erase(item);
	if (this->buckets[value].empty()) this->buckets.erase(value);
	this->items.erase(item);
}

int32 AIAbstractList::Begin()
{
	this->initialized = true;
	return this->sorter->Begin();
}

int32 AIAbstractList::Next()
{
	if (this->initialized == false) {
		DEBUG(ai, 0, "ERROR: Next() is invalid as Begin() is never called");
		return false;
	}
	return this->sorter->Next();
}

bool AIAbstractList::IsEmpty()
{
	return this->items.empty();
}

bool AIAbstractList::HasNext()
{
	if (this->initialized == false) {
		DEBUG(ai, 0, "ERROR: HasNext() is invalid as Begin() is never called");
		return false;
	}
	return this->sorter->HasNext();
}

int32 AIAbstractList::Count()
{
	return (int32)this->items.size();
}

int32 AIAbstractList::GetValue(int32 item)
{
	if (!this->HasItem(item)) return 0;

	return this->items[item];
}

bool AIAbstractList::SetValue(int32 item, int32 value)
{
	if (!this->HasItem(item)) return false;

	int32 value_old = this->GetValue(item);

	this->sorter->Remove(item);
	this->buckets[value_old].erase(item);
	if (this->buckets[value_old].empty()) this->buckets.erase(value_old);
	this->items[item] = value;
	this->buckets[value].insert(item);

	return true;
}

void AIAbstractList::Sort(SorterType sorter, bool ascending)
{
	if (sorter != SORT_BY_VALUE && sorter != SORT_BY_ITEM) return;
	if (sorter == this->sorter_type && ascending == this->sort_ascending) return;

	delete this->sorter;
	switch (sorter) {
		case SORT_BY_ITEM:
			if (ascending) this->sorter = new AIAbstractListSorterItemAscending(this);
			else           this->sorter = new AIAbstractListSorterItemDescending(this);
			break;

		case SORT_BY_VALUE:
			if (ascending) this->sorter = new AIAbstractListSorterValueAscending(this);
			else           this->sorter = new AIAbstractListSorterValueDescending(this);
			break;

		default:
			this->Sort(SORT_BY_ITEM, false);
			return;
	}
	this->sorter_type    = sorter;
	this->sort_ascending = ascending;
}

void AIAbstractList::AddList(AIAbstractList *list)
{
	AIAbstractListMap *list_items = &list->items;
	for (AIAbstractListMap::iterator iter = list_items->begin(); iter != list_items->end(); iter++) {
		this->AddItem((*iter).first);
		this->SetValue((*iter).first, (*iter).second);
	}
}

void AIAbstractList::RemoveAboveValue(int32 value)
{
	for (AIAbstractListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second > value) this->items.erase(iter);
	}

	for (AIAbstractListBucket::iterator next_iter, iter = this->buckets.begin(); iter != this->buckets.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).first > value) this->buckets.erase(iter);
	}
}

void AIAbstractList::RemoveBelowValue(int32 value)
{
	for (AIAbstractListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second < value) this->items.erase(iter);
	}

	for (AIAbstractListBucket::iterator next_iter, iter = this->buckets.begin(); iter != this->buckets.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).first < value) this->buckets.erase(iter);
	}
}

void AIAbstractList::RemoveBetweenValue(int32 start, int32 end)
{
	for (AIAbstractListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second > start && (*iter).second < end) this->items.erase(iter);
	}

	for (AIAbstractListBucket::iterator next_iter, iter = this->buckets.begin(); iter != this->buckets.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).first > start && (*iter).first < end) this->buckets.erase(iter);
	}
}

void AIAbstractList::RemoveValue(int32 value)
{
	for (AIAbstractListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second == value) this->items.erase(iter);
	}

	for (AIAbstractListBucket::iterator next_iter, iter = this->buckets.begin(); iter != this->buckets.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).first == value) this->buckets.erase(iter);
	}
}

void AIAbstractList::RemoveTop(int32 count)
{
	if (!this->sort_ascending) {
		this->Sort(this->sorter_type, !this->sort_ascending);
		this->RemoveBottom(count);
		this->Sort(this->sorter_type, !this->sort_ascending);
		return;
	}

	switch (this->sorter_type) {
		default: NOT_REACHED();
		case SORT_BY_VALUE:
			for (AIAbstractListBucket::iterator iter = this->buckets.begin(); iter != this->buckets.end(); iter = this->buckets.begin()) {
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
			for (AIAbstractListMap::iterator iter = this->items.begin(); iter != this->items.end(); iter = this->items.begin()) {
				if (--count < 0) return;
				this->RemoveItem((*iter).first);
			}
			break;
	}
}

void AIAbstractList::RemoveBottom(int32 count)
{
	if (!this->sort_ascending) {
		this->Sort(this->sorter_type, !this->sort_ascending);
		this->RemoveTop(count);
		this->Sort(this->sorter_type, !this->sort_ascending);
		return;
	}

	switch (this->sorter_type) {
		default: NOT_REACHED();
		case SORT_BY_VALUE:
			for (AIAbstractListBucket::reverse_iterator iter = this->buckets.rbegin(); iter != this->buckets.rend(); iter = this->buckets.rbegin()) {
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
			for (AIAbstractListMap::reverse_iterator iter = this->items.rbegin(); iter != this->items.rend(); iter = this->items.rbegin()) {
				if (--count < 0) return;
				this->RemoveItem((*iter).first);
			}
			break;
	}
}

void AIAbstractList::RemoveList(AIAbstractList *list)
{
	AIAbstractListMap *list_items = &list->items;
	for (AIAbstractListMap::iterator iter = list_items->begin(); iter != list_items->end(); iter++) {
		this->RemoveItem((*iter).first);
	}
}

void AIAbstractList::KeepAboveValue(int32 value)
{
	for (AIAbstractListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second <= value) this->items.erase(iter);
	}

	for (AIAbstractListBucket::iterator next_iter, iter = this->buckets.begin(); iter != this->buckets.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).first <= value) this->buckets.erase(iter);
	}
}

void AIAbstractList::KeepBelowValue(int32 value)
{
	for (AIAbstractListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second >= value) this->items.erase(iter);
	}

	for (AIAbstractListBucket::iterator next_iter, iter = this->buckets.begin(); iter != this->buckets.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).first >= value) this->buckets.erase(iter);
	}
}

void AIAbstractList::KeepBetweenValue(int32 start, int32 end)
{
	for (AIAbstractListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second <= start || (*iter).second >= end) this->items.erase(iter);
	}

	for (AIAbstractListBucket::iterator next_iter, iter = this->buckets.begin(); iter != this->buckets.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).first <= start || (*iter).first >= end) this->buckets.erase(iter);
	}
}

void AIAbstractList::KeepValue(int32 value)
{
	for (AIAbstractListMap::iterator next_iter, iter = this->items.begin(); iter != this->items.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).second != value) this->items.erase(iter);
	}

	for (AIAbstractListBucket::iterator next_iter, iter = this->buckets.begin(); iter != this->buckets.end(); iter = next_iter) {
		next_iter = iter; next_iter++;
		if ((*iter).first != value) this->buckets.erase(iter);
	}
}

void AIAbstractList::KeepTop(int32 count)
{
	this->RemoveBottom(this->Count() - count);
}

void AIAbstractList::KeepBottom(int32 count)
{
	this->RemoveTop(this->Count() - count);
}

void AIAbstractList::KeepList(AIAbstractList *list)
{
	AIAbstractList tmp;
	for (AIAbstractListMap::iterator iter = this->items.begin(); iter != this->items.end(); iter++) {
		tmp.AddItem((*iter).first);
		tmp.SetValue((*iter).first, (*iter).second);
	}

	tmp.RemoveList(list);
	this->RemoveList(&tmp);
}

SQInteger AIAbstractList::_get(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	if (!this->HasItem(idx)) return SQ_ERROR;

	sq_pushinteger(vm, this->GetValue(idx));
	return 1;
}

SQInteger AIAbstractList::_nexti(HSQUIRRELVM vm)
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
	if (!this->HasNext()) {
		sq_pushnull(vm);
		return 1;
	}

	sq_pushinteger(vm, val);
	return 1;
}

SQInteger AIAbstractList::Valuate(HSQUIRRELVM vm)
{
	/* The first parameter is the instance of AIAbstractList. */
	int nparam = sq_gettop(vm) - 1;

	if (nparam < 1) {
		return sq_throwerror(vm, _SC("You need to give a least a Valuator as parameter to AIAbstractList::Valuate"));
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

	/* Walk all items, and query the result */
	this->buckets.clear();
	for (AIAbstractListMap::iterator iter = this->items.begin(); iter != this->items.end(); iter++) {
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
			} break;

			case OT_BOOL: {
				SQBool v;
				sq_getbool(vm, -1, &v);
				value = v ? 1 : 0;
			} break;

			default: {
				/* See below for explanation. The extra pop is the return value. */
				sq_pop(vm, nparam + 4);

				AIObject::SetAllowDoCommand(backup_allow);
				return sq_throwerror(vm, _SC("return value of valuator is not valid (not integer/bool)"));
			}
		}

		(*iter).second = (int32)value;
		this->buckets[(int32)value].insert((*iter).first);

		/* Pop the return value. */
		sq_poptop(vm);

		Squirrel::DecreaseOps(vm, 5);
	}
	/* Pop from the squirrel stack:
	 * 1. The root stable (as instance object).
	 * 2. The valuator function.
	 * 3. The parameters given to this function.
	 * 4. The AIAbstractList instance object. */
	sq_pop(vm, nparam + 3);

	AIObject::SetAllowDoCommand(backup_allow);
	return 0;
}
