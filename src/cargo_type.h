/* $Id$ */

/** @file cargo_type.h Types related to cargos... */

#ifndef CARGO_TYPE_H
#define CARGO_TYPE_H

#include "core/enum_type.hpp"

typedef byte CargoID;

/** Available types of cargo */
enum CargoTypes {
	/* Temperate */
	CT_PASSENGERS   =  0,
	CT_COAL         =  1,
	CT_MAIL         =  2,
	CT_OIL          =  3,
	CT_LIVESTOCK    =  4,
	CT_GOODS        =  5,
	CT_GRAIN        =  6,
	CT_WOOD         =  7,
	CT_IRON_ORE     =  8,
	CT_STEEL        =  9,
	CT_VALUABLES    = 10,

	/* Arctic */
	CT_WHEAT        =  6,
	CT_HILLY_UNUSED =  8,
	CT_PAPER        =  9,
	CT_GOLD         = 10,
	CT_FOOD         = 11,

	/* Tropic */
	CT_RUBBER       =  1,
	CT_FRUIT        =  4,
	CT_MAIZE        =  6,
	CT_COPPER_ORE   =  8,
	CT_WATER        =  9,
	CT_DIAMONDS     = 10,

	/* Toyland */
	CT_SUGAR        =  1,
	CT_TOYS         =  3,
	CT_BATTERIES    =  4,
	CT_CANDY        =  5,
	CT_TOFFEE       =  6,
	CT_COLA         =  7,
	CT_COTTON_CANDY =  8,
	CT_BUBBLES      =  9,
	CT_PLASTIC      = 10,
	CT_FIZZY_DRINKS = 11,

	NUM_CARGO       = 32,

	CT_NO_REFIT     = 0xFE,
	CT_INVALID      = 0xFF
};

/** Class for storing amounts of cargo */
struct CargoArray {
private:
	uint amount[NUM_CARGO];

public:
	FORCEINLINE CargoArray()
	{
		this->Clear();
	}

	FORCEINLINE void Clear()
	{
		memset(this->amount, 0, sizeof(this->amount));
	}

	FORCEINLINE uint &operator[](CargoID cargo)
	{
		return this->amount[cargo];
	}

	FORCEINLINE const uint &operator[](CargoID cargo) const
	{
		return this->amount[cargo];
	}
};


/** Types of cargo source and destination */
enum SourceType {
	ST_INDUSTRY,     ///< Source/destination is an industry
	ST_TOWN,         ///< Source/destination is a town
	ST_HEADQUARTERS, ///< Source/destination are company headquarters
};
typedef SimpleTinyEnumT<SourceType, byte> SourceTypeByte;

typedef uint16 SourceID; ///< Contains either industry ID, town ID or company ID (or INVALID_SOURCE)
static const SourceID INVALID_SOURCE = 0xFFFF; ///< Invalid/unknown index of source

#endif /* CARGO_TYPE_H */
