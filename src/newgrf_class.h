/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_class.h Header file for classes to be used by e.g. NewGRF stations and airports */

#ifndef NEWGRF_CLASS_H
#define NEWGRF_CLASS_H

#include "strings_type.h"


/**
 * Struct containing information relating to NewGRF classes for stations and airports.
 */
template <typename Tspec, typename Tid, Tid Tmax>
struct NewGRFClass {
private:
	uint ui_count;             ///< Number of specs in this class potentially available to the user.
	std::vector<Tspec *> spec; ///< List of specifications.

	/**
	 * The actual classes.
	 * @note We store pointers to members of this array in various places outside this class (e.g. to 'name' for GRF string resolving).
	 *       Thus this must be a static array, and cannot be a self-resizing vector or similar.
	 */
	static NewGRFClass<Tspec, Tid, Tmax> classes[Tmax];

	void ResetClass();

	/** Initialise the defaults. */
	static void InsertDefaults();

public:
	uint32_t global_id; ///< Global ID for class, e.g. 'DFLT', 'WAYP', etc.
	StringID name;    ///< Name of this class.

	void Insert(Tspec *spec);

	/** Get the number of allocated specs within the class. */
	uint GetSpecCount() const { return static_cast<uint>(this->spec.size()); }
	/** Get the number of potentially user-available specs within the class. */
	uint GetUISpecCount() const { return this->ui_count; }
	int GetUIFromIndex(int index) const;
	int GetIndexFromUI(int ui_index) const;

	const Tspec *GetSpec(uint index) const;

	/** Check whether the spec will be available to the user at some point in time. */
	bool IsUIAvailable(uint index) const;

	static void Reset();
	static Tid Allocate(uint32_t global_id);
	static void Assign(Tspec *spec);
	static uint GetClassCount();
	static uint GetUIClassCount();
	static Tid GetUIClass(uint index);
	static NewGRFClass *Get(Tid cls_id);

	static const Tspec *GetByGrf(uint32_t grfid, uint16_t local_id, int *index);
};

#endif /* NEWGRF_CLASS_H */
