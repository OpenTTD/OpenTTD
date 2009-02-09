/* $Id$ */

/** @file company_func.h Functions related to companies. */

#ifndef COMPANY_FUNC_H
#define COMPANY_FUNC_H

#include "core/math_func.hpp"
#include "company_type.h"
#include "tile_type.h"
#include "strings_type.h"
#include "gfx_type.h"

void ChangeOwnershipOfCompanyItems(Owner old_owner, Owner new_owner);
void GetNameOfOwner(Owner owner, TileIndex tile);
void SetLocalCompany(CompanyID new_company);

extern CompanyByte _local_company;
extern CompanyByte _current_company;

extern Colours _company_colours[MAX_COMPANIES];  ///< NOSAVE: can be determined from company structs
extern CompanyManagerFace _company_manager_face; ///< for company manager face storage in openttd.cfg

bool IsHumanCompany(CompanyID company);

static inline bool IsLocalCompany()
{
	return _local_company == _current_company;
}

static inline bool IsInteractiveCompany(CompanyID company)
{
	return company == _local_company;
}

#endif /* COMPANY_FUNC_H */
