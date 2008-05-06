/* $Id$ */

/** @file helpers.cpp Some not-so-generic helper functions. */

#include "stdafx.h"

#include "openttd.h"
#include "engine_func.h"

#include <new>
#include "misc/blob.hpp"

/* Engine list manipulators - current implementation is only C wrapper around CBlobT<EngineID> (see yapf/blob.hpp) */

/* we cannot expose CBlobT directly to C so we must cast EngineList* to CBlobT<EngineID>* always when we are called from C */
#define B (*(CBlobT<EngineID>*)el)

/** Create Engine List (and initialize it to empty)
 * @param el list to be created
 */
void EngList_Create(EngineList *el)
{
	/* call CBlobT constructor explicitly */
	new (&B) CBlobT<EngineID>();
}

/** Destroy Engine List (and free its contents)
 * @param el list to be destroyed
 */
void EngList_Destroy(EngineList *el)
{
	/* call CBlobT destructor explicitly */
	B.~CBlobT<EngineID>();
}

/** Return number of items stored in the Engine List
 * @param el list for count inquiry
 * @return the desired count
 */
uint EngList_Count(const EngineList *el)
{
	return B.Size();
}

/** Add new item at the end of Engine List
 * @param el list o which to add an engine
 * @param eid engine to add to the list
 */
void EngList_Add(EngineList *el, EngineID eid)
{
	B.Append(eid);
}

/** Return pointer to the items array held by Engine List
 * @param el list from which the array pointer has to be returned
 * @return the pointer required
 */
EngineID* EngList_Items(EngineList *el)
{
	return B.Data();
}

/** Clear the Engine List (by invalidating all its items == reseting item count to zero)
 * @param el list to be cleared
 */
void EngList_RemoveAll(EngineList *el)
{
	B.Clear();
}

/** Sort all items using qsort() and given 'CompareItems' function
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 */
void EngList_Sort(EngineList *el, EngList_SortTypeFunction compare)
{
	qsort(B.Data(), B.Size(), sizeof(**el), compare);
}

/** Sort selected range of items (on indices @ <begin, begin+num_items-1>)
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 * @param begin start of sorting
 * @param num_items count of items to be sorted
 */
void EngList_SortPartial(EngineList *el, EngList_SortTypeFunction compare, uint begin, uint num_items)
{
	assert(begin <= (uint)B.Size());
	assert(begin + num_items <= (uint)B.Size());
	qsort(B.Data() + begin, num_items, sizeof(**el), compare);
}

#undef B

