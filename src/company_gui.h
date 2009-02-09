/* $Id$ */

/** @file company_gui.h GUI Functions related to companies. */

#ifndef COMPANY_GUI_H
#define COMPANY_GUI_H

#include "company_type.h"

uint16 GetDrawStringCompanyColour(CompanyID company);
void DrawCompanyIcon(CompanyID c, int x, int y);

void ShowCompanyStations(CompanyID company);
void ShowCompanyFinances(CompanyID company);
void ShowCompany(CompanyID company);

void InvalidateCompanyWindows(const Company *c);
void DeleteCompanyWindows(CompanyID company);

#endif /* COMPANY_GUI_H */
