/* $Id$ */

/** @file subsidy_base.h Subsidy base class. */

#ifndef SUBSIDY_BASE_H
#define SUBSIDY_BASE_H

#include "cargo_type.h"
#include "company_type.h"

typedef uint16 SubsidyID; ///< ID of a subsidy

/** Struct about subsidies, offered and awarded */
struct Subsidy {
	CargoID cargo_type; ///< Cargo type involved in this subsidy, CT_INVALID for invalid subsidy
	byte age;           ///< Subsidy age; < 12 is unawarded, >= 12 is awarded
	uint16 from;        ///< Index of source. Either TownID, IndustryID or StationID, when awarded
	uint16 to;          ///< Index of destination. Either TownID, IndustryID or StationID, when awarded

	/**
	 * Determines index of this subsidy
	 * @return index (in the Subsidy::array array)
	 */
	FORCEINLINE SubsidyID Index() const
	{
		return this - Subsidy::array;
	}

	/**
	 * Tests for validity of this subsidy
	 * @return is this subsidy valid?
	 */
	FORCEINLINE bool IsValid() const
	{
		return this->cargo_type != CT_INVALID;
	}


	static Subsidy array[MAX_COMPANIES]; ///< Array holding all subsidies

	/**
	 * Total number of subsidies, both valid and invalid
	 * @return length of Subsidy::array
	 */
	static FORCEINLINE size_t GetArraySize()
	{
		return lengthof(Subsidy::array);
	}

	/**
	 * Tests whether given index is an index of valid subsidy
	 * @param index index to check
	 * @return can this index be used to access a valid subsidy?
	 */
	static FORCEINLINE bool IsValidID(size_t index)
	{
		return index < Subsidy::GetArraySize() && Subsidy::Get(index)->IsValid();
	}

	/**
	 * Returns pointer to subsidy with given index
	 * @param index index of subsidy
	 * @return pointer to subsidy with given index
	 */
	static FORCEINLINE Subsidy *Get(size_t index)
	{
		assert(index < Subsidy::GetArraySize());
		return &Subsidy::array[index];
	}

	static Subsidy *AllocateItem();
	static void Clean();
};

#define FOR_ALL_SUBSIDIES_FROM(var, start) for (size_t subsidy_index = start; var = NULL, subsidy_index < Subsidy::GetArraySize(); subsidy_index++) \
		if ((var = Subsidy::Get(subsidy_index))->IsValid())
#define FOR_ALL_SUBSIDIES(var) FOR_ALL_SUBSIDIES_FROM(var, 0)

#endif /* SUBSIDY_BASE_H */
