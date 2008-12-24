/* $Id$ */

/** @file company_type.h Types related to companies. */

#ifndef COMPANY_TYPE_H
#define COMPANY_TYPE_H

#include "core/enum_type.hpp"

/**
 * Enum for all companies/owners.
 */
enum Owner {
	/* All companies below MAX_COMPANIES are playable
	 * companies, above, they are special, computer controlled 'companies' */
	OWNER_BEGIN     = 0x00, ///< First owner
	COMPANY_FIRST   = 0x00, ///< First company, same as owner
	MAX_COMPANIES   = 0x0F, ///< Maximum number of companies
	OWNER_TOWN      = 0x0F, ///< A town owns the tile, or a town is expanding
	OWNER_NONE      = 0x10, ///< The tile has no ownership
	OWNER_WATER     = 0x11, ///< The tile/execution is done by "water"
	OWNER_END,              ///< Last + 1 owner
	INVALID_OWNER   = 0xFF, ///< An invalid owner
	INVALID_COMPANY = 0xFF, ///< An invalid company

	/* 'Fake' companies used for networks */
	COMPANY_INACTIVE_CLIENT = 253, ///< The client is joining
	COMPANY_NEW_COMPANY     = 254, ///< The client wants a new company
	COMPANY_SPECTATOR       = 255, ///< The client is spectating
};
DECLARE_POSTFIX_INCREMENT(Owner);

enum {
	MAX_LENGTH_PRESIDENT_NAME_BYTES  =  31, ///< The maximum length of a president name in bytes including '\0'
	MAX_LENGTH_PRESIDENT_NAME_PIXELS =  94, ///< The maximum length of a president name in pixels
	MAX_LENGTH_COMPANY_NAME_BYTES    =  31, ///< The maximum length of a company name in bytes including '\0'
	MAX_LENGTH_COMPANY_NAME_PIXELS   = 150, ///< The maximum length of a company name in pixels
};

/** Define basic enum properties */
template <> struct EnumPropsT<Owner> : MakeEnumPropsT<Owner, byte, OWNER_BEGIN, OWNER_END, INVALID_OWNER> {};
typedef TinyEnumT<Owner> OwnerByte;

typedef Owner CompanyID;
typedef OwnerByte CompanyByte;

typedef uint16 CompanyMask;

struct Company;
typedef uint32 CompanyManagerFace; ///< Company manager face bits, info see in company_manager_face.h

#endif /* COMPANY_TYPE_H */
