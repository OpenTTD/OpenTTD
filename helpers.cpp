#include "stdafx.h"

EXTERN_C_BEGIN
#include "openttd.h"
#include "engine.h"
EXTERN_C_END

#include <new>
#include "yapf/blob.hpp"

/* Engine list manipulators - current implementation is only C wrapper around CBlobT<EngineID> (see yapf/blob.hpp) */

/* we cannot expose CBlobT directly to C so we must cast EngineList* to CBlobT<EngineID>* always when we are called from C */
#define B (*(CBlobT<EngineID>*)el)

/** Create Engine List (and initialize it to empty) */
void EngList_Create(EngineList *el)
{
	// call CBlobT constructor explicitly
	new (&B) CBlobT<EngineID>();
}

/** Destroy Engine List (and free its contents) */
void EngList_Destroy(EngineList *el)
{
	// call CBlobT destructor explicitly
	B.~CBlobT<EngineID>();
}

/** Return number of items stored in the Engine List */
uint EngList_Count(const EngineList *el)
{
	return B.Size();
}

/** Add new item at the end of Engine List */
void EngList_Add(EngineList *el, EngineID eid)
{
	B.Append(eid);
}

/** Return pointer to the items array held by Engine List */
EngineID* EngList_Items(EngineList *el)
{
	return B.Data();
}

/** Clear the Engine List (by invalidating all its items == reseting item count to zero) */
void EngList_RemoveAll(EngineList *el)
{
	B.Clear();
}

/** Sort all items using qsort() and given 'CompareItems' function */
void EngList_Sort(EngineList *el, EngList_SortTypeFunction compare)
{
	qsort(B.Data(), B.Size(), sizeof(**el), compare);
}

/** Sort selected range of items (on indices @ <begin, begin+num_items-1>) */
void EngList_SortPartial(EngineList *el, EngList_SortTypeFunction compare, uint begin, uint num_items)
{
	assert(begin <= (uint)B.Size());
	assert(begin + num_items <= (uint)B.Size());
	qsort(B.Data() + begin, num_items, sizeof(**el), compare);
}

#undef B

