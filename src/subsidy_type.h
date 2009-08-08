/* $Id$ */

/** @file subsidy_type.h basic types related to subsidies */

#ifndef SUBSIDY_TYPE_H
#define SUBSIDY_TYPE_H

#include "core/enum_type.hpp"

enum PartOfSubsidy {
	POS_NONE =     0,
	POS_SRC = 1 << 0, ///< bit 0 set -> town/industry is source of subsidised path
	POS_DST = 1 << 1, ///< bit 1 set -> town/industry is destination of subsidised path
};
typedef SimpleTinyEnumT<PartOfSubsidy, byte> PartOfSubsidyByte;

DECLARE_ENUM_AS_BIT_SET(PartOfSubsidy);

typedef uint16 SubsidyID; ///< ID of a subsidy
struct Subsidy;

#endif /* SUBSIDY_TYPE_H */
