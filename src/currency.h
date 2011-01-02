/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file currency.h Functions to handle different currencies. */

#ifndef CURRENCY_H
#define CURRENCY_H

#include "date_type.h"
#include "strings_type.h"

static const int CF_NOEURO = 0;
static const int CF_ISEURO = 1;
static const uint NUM_CURRENCY = 29;
static const int CUSTOM_CURRENCY_ID = NUM_CURRENCY - 1;

struct CurrencySpec {
	uint16 rate;
	char separator[8];
	Year to_euro;
	char prefix[16];
	char suffix[16];
	/**
	 * The currency symbol is represented by two possible values, prefix and suffix
	 * Usage of one or the other is determined by symbol_pos.
	 * 0 = prefix
	 * 1 = suffix
	 * 2 = both : Special case only for custom currency.
	 *            It is not a spec from Newgrf,
	 *            rather a way to let users do what they want with custom curency
	 */
	byte symbol_pos;
	StringID name;
};


extern CurrencySpec _currency_specs[NUM_CURRENCY];

/* XXX small hack, but makes the rest of the code a bit nicer to read */
#define _custom_currency (_currency_specs[CUSTOM_CURRENCY_ID])
#define _currency ((const CurrencySpec*)&_currency_specs[GetGameSettings().locale.currency])

uint GetMaskOfAllowedCurrencies();
void CheckSwitchToEuro();
void ResetCurrencies(bool preserve_custom = true);
StringID *BuildCurrencyDropdown();
byte GetNewgrfCurrencyIdConverted(byte grfcurr_id);

#endif /* CURRENCY_H */
