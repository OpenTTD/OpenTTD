/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_class_func.h Implementation of the NewGRF class' functions. */

#include "newgrf_class.h"

#include "table/strings.h"

/** Instantiate the array. */
template <typename Tspec, typename Tid, Tid Tmax>
NewGRFClass<Tspec, Tid, Tmax> NewGRFClass<Tspec, Tid, Tmax>::classes[Tmax];

/** Reset the class, i.e. clear everything. */
template <typename Tspec, typename Tid, Tid Tmax>
void NewGRFClass<Tspec, Tid, Tmax>::ResetClass()
{
	this->global_id = 0;
	this->name      = STR_EMPTY;
	this->ui_count  = 0;

	this->spec.clear();
}

/** Reset the classes, i.e. clear everything. */
template <typename Tspec, typename Tid, Tid Tmax>
void NewGRFClass<Tspec, Tid, Tmax>::Reset()
{
	for (Tid i = (Tid)0; i < Tmax; i++) {
		classes[i].ResetClass();
	}

	InsertDefaults();
}

/**
 * Allocate a class with a given global class ID.
 * @param cls_id The global class id, such as 'DFLT'.
 * @return The (non global!) class ID for the class.
 * @note Upon allocating the same global class ID for a
 *       second time, this first allocation will be given.
 */
template <typename Tspec, typename Tid, Tid Tmax>
Tid NewGRFClass<Tspec, Tid, Tmax>::Allocate(uint32_t global_id)
{
	for (Tid i = (Tid)0; i < Tmax; i++) {
		if (classes[i].global_id == global_id) {
			/* ClassID is already allocated, so reuse it. */
			return i;
		} else if (classes[i].global_id == 0) {
			/* This class is empty, so allocate it to the global id. */
			classes[i].global_id = global_id;
			return i;
		}
	}

	GrfMsg(2, "ClassAllocate: already allocated {} classes, using default", Tmax);
	return (Tid)0;
}

/**
 * Insert a spec into the class.
 * @param spec The spec to insert.
 */
template <typename Tspec, typename Tid, Tid Tmax>
void NewGRFClass<Tspec, Tid, Tmax>::Insert(Tspec *spec)
{
	this->spec.push_back(spec);

	if (this->IsUIAvailable(static_cast<uint>(this->spec.size() - 1))) this->ui_count++;
}

/**
 * Assign a spec to one of the classes.
 * @param spec The spec to assign.
 * @note The spec must have a valid class id set.
 */
template <typename Tspec, typename Tid, Tid Tmax>
void NewGRFClass<Tspec, Tid, Tmax>::Assign(Tspec *spec)
{
	assert(spec->cls_id < Tmax);
	Get(spec->cls_id)->Insert(spec);
}

/**
 * Get a particular class.
 * @param cls_id The id for the class.
 * @pre cls_id < Tmax
 */
template <typename Tspec, typename Tid, Tid Tmax>
NewGRFClass<Tspec, Tid, Tmax> *NewGRFClass<Tspec, Tid, Tmax>::Get(Tid cls_id)
{
	assert(cls_id < Tmax);
	return classes + cls_id;
}

/**
 * Get the number of allocated classes.
 * @return The number of classes.
 */
template <typename Tspec, typename Tid, Tid Tmax>
uint NewGRFClass<Tspec, Tid, Tmax>::GetClassCount()
{
	uint i;
	for (i = 0; i < Tmax && classes[i].global_id != 0; i++) {}
	return i;
}

/**
 * Get the number of classes available to the user.
 * @return The number of classes.
 */
template <typename Tspec, typename Tid, Tid Tmax>
uint NewGRFClass<Tspec, Tid, Tmax>::GetUIClassCount()
{
	uint cnt = 0;
	for (uint i = 0; i < Tmax && classes[i].global_id != 0; i++) {
		if (classes[i].GetUISpecCount() > 0) cnt++;
	}
	return cnt;
}

/**
 * Get the nth-class with user available specs.
 * @param index UI index of a class.
 * @return The class ID of the class.
 */
template <typename Tspec, typename Tid, Tid Tmax>
Tid NewGRFClass<Tspec, Tid, Tmax>::GetUIClass(uint index)
{
	for (uint i = 0; i < Tmax && classes[i].global_id != 0; i++) {
		if (classes[i].GetUISpecCount() == 0) continue;
		if (index-- == 0) return (Tid)i;
	}
	NOT_REACHED();
}

/**
 * Get a spec from the class at a given index.
 * @param index  The index where to find the spec.
 * @return The spec at given location.
 */
template <typename Tspec, typename Tid, Tid Tmax>
const Tspec *NewGRFClass<Tspec, Tid, Tmax>::GetSpec(uint index) const
{
	/* If the custom spec isn't defined any more, then the GRF file probably was not loaded. */
	return index < this->GetSpecCount() ? this->spec[index] : nullptr;
}

/**
 * Translate a UI spec index into a spec index.
 * @param ui_index UI index of the spec.
 * @return index of the spec, or -1 if out of range.
 */
template <typename Tspec, typename Tid, Tid Tmax>
int NewGRFClass<Tspec, Tid, Tmax>::GetIndexFromUI(int ui_index) const
{
	if (ui_index < 0) return -1;
	for (uint i = 0; i < this->GetSpecCount(); i++) {
		if (!this->IsUIAvailable(i)) continue;
		if (ui_index-- == 0) return i;
	}
	return -1;
}

/**
 * Translate a spec index into a UI spec index.
 * @param index index of the spec.
 * @return UI index of the spec, or -1 if out of range.
 */
template <typename Tspec, typename Tid, Tid Tmax>
int NewGRFClass<Tspec, Tid, Tmax>::GetUIFromIndex(int index) const
{
	if ((uint)index >= this->GetSpecCount()) return -1;
	uint ui_index = 0;
	for (int i = 0; i < index; i++) {
		if (this->IsUIAvailable(i)) ui_index++;
	}
	return ui_index;
}

/**
 * Retrieve a spec by GRF location.
 * @param grfid    GRF ID of spec.
 * @param local_id Index within GRF file of spec.
 * @param index    Pointer to return the index of the spec in its class. If nullptr then not used.
 * @return The spec.
 */
template <typename Tspec, typename Tid, Tid Tmax>
const Tspec *NewGRFClass<Tspec, Tid, Tmax>::GetByGrf(uint32_t grfid, uint16_t local_id, int *index)
{
	uint j;

	for (Tid i = (Tid)0; i < Tmax; i++) {
		uint count = static_cast<uint>(classes[i].spec.size());
		for (j = 0; j < count; j++) {
			const Tspec *spec = classes[i].spec[j];
			if (spec == nullptr) continue;
			if (spec->grf_prop.grffile->grfid == grfid && spec->grf_prop.local_id == local_id) {
				if (index != nullptr) *index = j;
				return spec;
			}
		}
	}

	return nullptr;
}
