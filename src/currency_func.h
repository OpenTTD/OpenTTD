/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file currency_func.h Functions to handle different currencies. */

#ifndef CURRENCY_FUNC_H
#define CURRENCY_FUNC_H

#include "currency_type.h"
#include "settings_type.h"

extern EnumIndexArray<CurrencySpec, Currency, Currency::End> _currency_specs;

/**
 * Get the custom currency.
 * @return Reference to custom currency.
 */
inline CurrencySpec &GetCustomCurrency()
{
	return _currency_specs[Currency::Custom];
}

/**
 * Get the currently selected currency.
 * @return Read-only reference to the current currency.
 */
inline const CurrencySpec &GetCurrency()
{
	return _currency_specs[GetGameSettings().locale.currency];
}

Currencies GetMaskOfAllowedCurrencies();
void ResetCurrencies(bool preserve_custom = true);
Currency GetNewgrfCurrencyIdConverted(uint8_t grfcurr_id);

#endif /* CURRENCY_FUNC_H */
