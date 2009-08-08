/* $Id$ */

/** @file subsidy_base.h Subsidy base class. */

#ifndef SUBSIDY_BASE_H
#define SUBSIDY_BASE_H

#include "cargo_type.h"
#include "company_type.h"
#include "subsidy_type.h"

/** Struct about subsidies, offered and awarded */
struct Subsidy {
	CargoID cargo_type;      ///< Cargo type involved in this subsidy, CT_INVALID for invalid subsidy
	byte remaining;          ///< Remaining months when this subsidy is valid
	CompanyByte awarded;     ///< Subsidy is awarded to this company; INVALID_COMPANY if it's not awarded to anyone
	SourceTypeByte src_type; ///< Source of subsidised path (ST_INDUSTRY or ST_TOWN)
	SourceTypeByte dst_type; ///< Destination of subsidised path (ST_INDUSTRY or ST_TOWN)
	SourceID src;            ///< Index of source. Either TownID or IndustryID
	SourceID dst;            ///< Index of destination. Either TownID or IndustryID

	/**
	 * Tests whether this subsidy has been awarded to someone
	 * @return is this subsidy awarded?
	 */
	FORCEINLINE bool IsAwarded() const
	{
		return this->awarded != INVALID_COMPANY;
	}

	void AwardTo(CompanyID company);

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
