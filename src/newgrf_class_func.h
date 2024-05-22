/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_class_func.h Implementation of the NewGRF class' functions. */

#include "newgrf_class.h"

#include "table/strings.h"

/** Reset the classes, i.e. clear everything. */
template <typename Tspec, typename Tindex, Tindex Tmax>
void NewGRFClass<Tspec, Tindex, Tmax>::Reset()
{
	NewGRFClass::classes.clear();
	NewGRFClass::classes.shrink_to_fit();

	NewGRFClass::InsertDefaults();
}

/**
 * Allocate a class with a given global class ID.
 * @param global_id The global class id, such as 'DFLT'.
 * @return The (non global!) class ID for the class.
 * @note Upon allocating the same global class ID for a
 *       second time, this first allocation will be given.
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
Tindex NewGRFClass<Tspec, Tindex, Tmax>::Allocate(uint32_t global_id)
{
	auto found = std::find_if(std::begin(NewGRFClass::classes), std::end(NewGRFClass::classes), [global_id](const auto &cls) { return cls.global_id == global_id; });

	/* Id is already allocated, so reuse it. */
	if (found != std::end(NewGRFClass::classes)) return found->Index();

	/* More slots available, allocate a slot to the global id. */
	if (NewGRFClass::classes.size() < Tmax) {
		auto it = NewGRFClass::classes.emplace(std::end(NewGRFClass::classes), global_id, STR_EMPTY);
		it->index = static_cast<Tindex>(std::distance(std::begin(NewGRFClass::classes), it));
		return it->Index();
	}

	GrfMsg(2, "ClassAllocate: already allocated {} classes, using default", Tmax);
	return static_cast<Tindex>(0);
}

/**
 * Insert a spec into the class, and update its index.
 * @param spec The spec to insert.
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
void NewGRFClass<Tspec, Tindex, Tmax>::Insert(Tspec *spec)
{
	auto it = this->spec.insert(std::end(this->spec), spec);
	uint16_t index = static_cast<uint16_t>(std::distance(std::begin(this->spec), it));
	if (spec != nullptr) (*it)->index = index;

	if (this->IsUIAvailable(index)) this->ui_count++;
}

/**
 * Assign a spec to one of the classes.
 * @param spec The spec to assign.
 * @note The spec must have a valid class index set.
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
void NewGRFClass<Tspec, Tindex, Tmax>::Assign(Tspec *spec)
{
	assert(static_cast<size_t>(spec->class_index) < NewGRFClass::classes.size());
	Get(spec->class_index)->Insert(spec);
}

/**
 * Get a particular class.
 * @param class_index The index of the class.
 * @pre class_index < Tmax
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
NewGRFClass<Tspec, Tindex, Tmax> *NewGRFClass<Tspec, Tindex, Tmax>::Get(Tindex class_index)
{
	assert(static_cast<size_t>(class_index) < NewGRFClass::classes.size());
	return &NewGRFClass::classes[class_index];
}

/**
 * Get the number of allocated classes.
 * @return The number of classes.
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
uint NewGRFClass<Tspec, Tindex, Tmax>::GetClassCount()
{
	return static_cast<uint>(NewGRFClass::classes.size());
}

/**
 * Get the number of classes available to the user.
 * @return The number of classes.
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
uint NewGRFClass<Tspec, Tindex, Tmax>::GetUIClassCount()
{
	return std::count_if(std::begin(NewGRFClass::classes), std::end(NewGRFClass::classes), [](const auto &cls) { return cls.GetUISpecCount() > 0; });
}

/**
 * Get a spec from the class at a given index.
 * @param index  The index where to find the spec.
 * @return The spec at given location.
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
const Tspec *NewGRFClass<Tspec, Tindex, Tmax>::GetSpec(uint index) const
{
	/* If the custom spec isn't defined any more, then the GRF file probably was not loaded. */
	return index < this->GetSpecCount() ? this->spec[index] : nullptr;
}

/**
 * Retrieve a spec by GRF location.
 * @param grfid    GRF ID of spec.
 * @param local_id Index within GRF file of spec.
 * @param index    Pointer to return the index of the spec in its class. If nullptr then not used.
 * @return The spec.
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
const Tspec *NewGRFClass<Tspec, Tindex, Tmax>::GetByGrf(uint32_t grfid, uint16_t local_id)
{
	for (const auto &cls : NewGRFClass::classes) {
		for (const auto &spec : cls.spec) {
			if (spec == nullptr) continue;
			if (spec->grf_prop.local_id != local_id) continue;
			if ((spec->grf_prop.grffile == nullptr ? 0 : spec->grf_prop.grffile->grfid) == grfid) return spec;
		}
	}

	return nullptr;
}
