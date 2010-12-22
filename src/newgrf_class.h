/* $Id$ */

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
	uint32 global_id; ///< Global ID for class, e.g. 'DFLT', 'WAYP', etc.
	StringID name;    ///< Name of this class.
	uint count;       ///< Number of stations in this class.
	Tspec **spec;     ///< Array of station specifications.

	/** The actual classes. */
	static NewGRFClass<Tspec, Tid, Tmax> classes[Tmax];

	/** Reset the classes, i.e. clear everything. */
	static void Reset();

	/** Initialise the defaults. */
	static void InsertDefaults();

	/**
	 * Allocate a class with a given global class ID.
	 * @param cls_id The global class id, such as 'DFLT'.
	 * @return The (non global!) class ID for the class.
	 * @note Upon allocating the same global class ID for a
	 *       second time, this first allocation will be given.
	 */
	static Tid Allocate(uint32 global_id);

	/**
	 * Set the name of a particular class.
	 * @param cls_id The id for the class.
	 * @pre index < GetCount(cls_id)
	 * @param name   The new name for the class.
	 */
	static void SetName(Tid cls_id, StringID name);

	/**
	 * Assign a spec to one of the classes.
	 * @param spec The spec to assign.
	 * @note The spec must have a valid class id set.
	 */
	static void Assign(Tspec *spec);


	/**
	 * Get the name of a particular class.
	 * @param cls_id The class to get the name of.
	 * @pre index < GetCount(cls_id)
	 * @return The name of said class.
	 */
	static StringID GetName(Tid cls_id);

	/**
	 * Get the number of allocated classes.
	 * @return The number of classes.
	 */
	static uint GetCount();

	/**
	 * Get the number of allocated specs within a particular class.
	 * @param cls_id The class to get the size of.
	 * @pre cls_id < GetCount()
	 * @return The size of the class.
	 */
	static uint GetCount(Tid cls_id);

	/**
	 * Get a spec from a particular class at a given index.
	 * @param cls_id The class to get the spec from.
	 * @param index  The index where to find the spec.
	 * @pre index < GetCount(cls_id)
	 * @return The spec at given location.
	 */
	static const Tspec *Get(Tid cls_id, uint index);

	/**
	 * Retrieve a spec by GRF location.
	 * @param grfid    GRF ID of spec.
	 * @param local_id Index within GRF file of spec.
	 * @param index    Pointer to return the index of the spec in its class. If NULL then not used.
	 * @return The spec.
	 */
	static const Tspec *GetByGrf(uint32 grfid, byte local_id, int *index);
};

#endif /* NEWGRF_CLASS_H */
