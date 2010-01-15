/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file company_func.h Functions related to companies. */

#ifndef COMPANY_FUNC_H
#define COMPANY_FUNC_H

#include "company_type.h"
#include "tile_type.h"
#include "gfx_type.h"

void ChangeOwnershipOfCompanyItems(Owner old_owner, Owner new_owner);
void GetNameOfOwner(Owner owner, TileIndex tile);
void SetLocalCompany(CompanyID new_company);
void ShowBuyCompanyDialog(CompanyID company);

extern CompanyByte _local_company;
extern CompanyByte _current_company;

extern Colours _company_colours[MAX_COMPANIES];  ///< NOSAVE: can be determined from company structs
extern CompanyManagerFace _company_manager_face; ///< for company manager face storage in openttd.cfg

static inline bool IsLocalCompany()
{
	return _local_company == _current_company;
}

static inline bool IsInteractiveCompany(CompanyID company)
{
	return company == _local_company;
}

#endif /* COMPANY_FUNC_H */
