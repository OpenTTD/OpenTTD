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
class NewGRFClass {
private:
	uint ui_count = 0; ///< Number of specs in this class potentially available to the user.
	std::vector<Tspec *> spec; ///< List of specifications.

	/**
	 * The actual classes.
	 * @note This may be reallocated during initialization so pointers may be invalidated.
	 */
	static inline std::vector<NewGRFClass<Tspec, Tid, Tmax>> classes;

	/** Initialise the defaults. */
	static void InsertDefaults();

public:
	uint32_t global_id; ///< Global ID for class, e.g. 'DFLT', 'WAYP', etc.
	StringID name;    ///< Name of this class.

	/* Public constructor as emplace_back needs access. */
	NewGRFClass(uint32_t global_id, StringID name) : global_id(global_id), name(name) { }

	/**
	 * Get read-only span of specs of this class.
	 * @return Read-only span of specs.
	 */
	std::span<Tspec * const> Specs() const { return this->spec; }

	/**
	 * Get read-only span of all classes of this type.
	 * @return Read-only span of classes.
	 */
	static std::span<NewGRFClass<Tspec, Tid, Tmax> const> Classes() { return NewGRFClass::classes; }

	void Insert(Tspec *spec);

	Tid Index() const { return static_cast<Tid>(std::distance(&*std::cbegin(NewGRFClass::classes), this)); }
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
