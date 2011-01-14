/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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
DECLARE_POSTFIX_INCREMENT(Owner)

static const uint MAX_LENGTH_PRESIDENT_NAME_CHARS  =  32; ///< The maximum length of a president name in characters including '\0'
static const uint MAX_LENGTH_PRESIDENT_NAME_PIXELS =  94; ///< The maximum length of a president name in pixels
static const uint MAX_LENGTH_COMPANY_NAME_CHARS    =  32; ///< The maximum length of a company name in characters including '\0'
static const uint MAX_LENGTH_COMPANY_NAME_PIXELS   = 150; ///< The maximum length of a company name in pixels

static const uint MAX_HISTORY_MONTHS               =  24; ///< The maximum number of months kept as performance's history

/** Define basic enum properties */
template <> struct EnumPropsT<Owner> : MakeEnumPropsT<Owner, byte, OWNER_BEGIN, OWNER_END, INVALID_OWNER> {};
typedef TinyEnumT<Owner> OwnerByte;

typedef Owner CompanyID;
typedef OwnerByte CompanyByte;

typedef uint16 CompanyMask;

struct Company;
typedef uint32 CompanyManagerFace; ///< Company manager face bits, info see in company_manager_face.h

#endif /* COMPANY_TYPE_H */
