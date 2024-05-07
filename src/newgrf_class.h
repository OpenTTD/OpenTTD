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

/* Base for each type of NewGRF spec to be used with NewGRFClass. */
template <typename Tindex>
struct NewGRFSpecBase {
	Tindex class_index; ///< Class index of this spec, invalid until class is allocated.
	uint16_t index; ///< Index within class of this spec, invalid until inserted into class.
};

/**
 * Struct containing information relating to NewGRF classes for stations and airports.
 */
template <typename Tspec, typename Tindex, Tindex Tmax>
class NewGRFClass {
private:
	/* Tspec must be of NewGRFSpecBase<Tindex>. */
	static_assert(std::is_base_of_v<NewGRFSpecBase<Tindex>, Tspec>);

	uint ui_count = 0; ///< Number of specs in this class potentially available to the user.
	Tindex index; ///< Index of class within the list of classes.
	std::vector<Tspec *> spec; ///< List of specifications.

	/**
	 * The actual classes.
	 * @note This may be reallocated during initialization so pointers may be invalidated.
	 */
	static inline std::vector<NewGRFClass<Tspec, Tindex, Tmax>> classes;

	/** Initialise the defaults. */
	static void InsertDefaults();

public:
	using spec_type = Tspec;
	using index_type = Tindex;

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
	static std::span<NewGRFClass<Tspec, Tindex, Tmax> const> Classes() { return NewGRFClass::classes; }

	void Insert(Tspec *spec);

	Tindex Index() const { return this->index; }
	/** Get the number of allocated specs within the class. */
	uint GetSpecCount() const { return static_cast<uint>(this->spec.size()); }
	/** Get the number of potentially user-available specs within the class. */
	uint GetUISpecCount() const { return this->ui_count; }

	const Tspec *GetSpec(uint index) const;

	/** Check whether the spec will be available to the user at some point in time. */
	bool IsUIAvailable(uint index) const;

	static void Reset();
	static Tindex Allocate(uint32_t global_id);
	static void Assign(Tspec *spec);
	static uint GetClassCount();
	static uint GetUIClassCount();
	static NewGRFClass *Get(Tindex class_index);

	static const Tspec *GetByGrf(uint32_t grfid, uint16_t local_id);
};

#endif /* NEWGRF_CLASS_H */
