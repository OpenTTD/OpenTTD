/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file currency.cpp Support for different currencies. */

#include "stdafx.h"
#include "core/bitmath_func.hpp"

#include "currency.h"
#include "news_func.h"
#include "settings_type.h"
#include "date_func.h"
#include "string_type.h"

#include "table/strings.h"

#include "safeguards.h"

	/*   exchange rate    prefix                         symbol_pos
	 *   |  separator        |           postfix             |
	 *   |   |   Euro year   |              |                | name
	 *   |   |    |          |              |                |  | */
/** The original currency specifications. */
static const CurrencySpec origin_currency_specs[CURRENCY_END] = {
	{    1, "", CF_NOEURO, "\xC2\xA3",     "",               0, STR_GAME_OPTIONS_CURRENCY_GBP    }, ///< british pound
	{    2, "", CF_NOEURO, "$",            "",               0, STR_GAME_OPTIONS_CURRENCY_USD    }, ///< american dollar
	{    2, "", CF_ISEURO, "\xE2\x82\xAC", "",               0, STR_GAME_OPTIONS_CURRENCY_EUR    }, ///< euro
	{  220, "", CF_NOEURO, "\xC2\xA5",     "",               0, STR_GAME_OPTIONS_CURRENCY_JPY    }, ///< japanese yen
	{   27, "", 2002,      "",             NBSP "S.",        1, STR_GAME_OPTIONS_CURRENCY_ATS    }, ///< austrian schilling
	{   81, "", 2002,      "BEF" NBSP,     "",               0, STR_GAME_OPTIONS_CURRENCY_BEF    }, ///< belgian franc
	{    2, "", CF_NOEURO, "CHF" NBSP,     "",               0, STR_GAME_OPTIONS_CURRENCY_CHF    }, ///< swiss franc
	{   41, "", CF_NOEURO, "",             NBSP "K\xC4\x8D", 1, STR_GAME_OPTIONS_CURRENCY_CZK    }, ///< czech koruna
	{    4, "", 2002,      "DM" NBSP,      "",               0, STR_GAME_OPTIONS_CURRENCY_DEM    }, ///< deutsche mark
	{   11, "", CF_NOEURO, "",             NBSP "kr",        1, STR_GAME_OPTIONS_CURRENCY_DKK    }, ///< danish krone
	{  333, "", 2002,      "Pts" NBSP,     "",               0, STR_GAME_OPTIONS_CURRENCY_ESP    }, ///< spanish peseta
	{   12, "", 2002,      "",             NBSP "mk",        1, STR_GAME_OPTIONS_CURRENCY_FIM    }, ///< finnish markka
	{   13, "", 2002,      "FF" NBSP,      "",               0, STR_GAME_OPTIONS_CURRENCY_FRF    }, ///< french franc
	{  681, "", 2002,      "",             "Dr.",            1, STR_GAME_OPTIONS_CURRENCY_GRD    }, ///< greek drachma
	{  378, "", CF_NOEURO, "",             NBSP "Ft",        1, STR_GAME_OPTIONS_CURRENCY_HUF    }, ///< hungarian forint
	{  130, "", CF_NOEURO, "",             NBSP "Kr",        1, STR_GAME_OPTIONS_CURRENCY_ISK    }, ///< icelandic krona
	{ 3873, "", 2002,      "",             NBSP "L.",        1, STR_GAME_OPTIONS_CURRENCY_ITL    }, ///< italian lira
	{    4, "", 2002,      "NLG" NBSP,     "",               0, STR_GAME_OPTIONS_CURRENCY_NLG    }, ///< dutch gulden
	{   12, "", CF_NOEURO, "",             NBSP "Kr",        1, STR_GAME_OPTIONS_CURRENCY_NOK    }, ///< norwegian krone
	{    6, "", CF_NOEURO, "",             NBSP "z\xC5\x82", 1, STR_GAME_OPTIONS_CURRENCY_PLN    }, ///< polish zloty
	{    5, "", CF_NOEURO, "",             NBSP "Lei",       1, STR_GAME_OPTIONS_CURRENCY_RON    }, ///< romanian leu
	{   50, "", CF_NOEURO, "",             NBSP "p",         1, STR_GAME_OPTIONS_CURRENCY_RUR    }, ///< russian rouble
	{  479, "", 2007,      "",             NBSP "SIT",       1, STR_GAME_OPTIONS_CURRENCY_SIT    }, ///< slovenian tolar
	{   13, "", CF_NOEURO, "",             NBSP "Kr",        1, STR_GAME_OPTIONS_CURRENCY_SEK    }, ///< swedish krona
	{    3, "", CF_NOEURO, "",             NBSP "TL",        1, STR_GAME_OPTIONS_CURRENCY_TRY    }, ///< turkish lira
	{   60, "", 2009,      "",             NBSP "Sk",        1, STR_GAME_OPTIONS_CURRENCY_SKK    }, ///< slovak koruna
	{    4, "", CF_NOEURO, "R$" NBSP,      "",               0, STR_GAME_OPTIONS_CURRENCY_BRL    }, ///< brazil real
	{   31, "", 2011,      "",             NBSP "EEK",       1, STR_GAME_OPTIONS_CURRENCY_EEK    }, ///< estonian krooni
	{    4, "", 2015,      "",             NBSP "Lt",        1, STR_GAME_OPTIONS_CURRENCY_LTL    }, ///< lithuanian litas
	{ 1850, "", CF_NOEURO, "\xE2\x82\xA9", "",               0, STR_GAME_OPTIONS_CURRENCY_KRW    }, ///< south korean won
	{   13, "", CF_NOEURO, "R" NBSP,       "",               0, STR_GAME_OPTIONS_CURRENCY_ZAR    }, ///< south african rand
	{    1, "", CF_NOEURO, "",             "",               2, STR_GAME_OPTIONS_CURRENCY_CUSTOM }, ///< custom currency (add further languages below)
	{    3, "", CF_NOEURO, "",             NBSP "GEL",       1, STR_GAME_OPTIONS_CURRENCY_GEL    }, ///< Georgian Lari
	{ 4901, "", CF_NOEURO, "",             NBSP "Rls",       1, STR_GAME_OPTIONS_CURRENCY_IRR    }, ///< Iranian Rial
	{   80, "", CF_NOEURO, "",             NBSP "rub",       1, STR_GAME_OPTIONS_CURRENCY_RUB    }, ///< New Russian Ruble
};

/** Array of currencies used by the system */
CurrencySpec _currency_specs[CURRENCY_END];

/**
 * This array represent the position of OpenTTD's currencies,
 * compared to TTDPatch's ones.
 * When a grf sends currencies, they are based on the order defined by TTDPatch.
 * So, we must reindex them to our own order.
 */
const byte TTDPatch_To_OTTDIndex[] =
{
	CURRENCY_GBP,
	CURRENCY_USD,
	CURRENCY_FRF,
	CURRENCY_DEM,
	CURRENCY_JPY,
	CURRENCY_ESP,
	CURRENCY_HUF,
	CURRENCY_PLN,
	CURRENCY_ATS,
	CURRENCY_BEF,
	CURRENCY_DKK,
	CURRENCY_FIM,
	CURRENCY_GRD,
	CURRENCY_CHF,
	CURRENCY_NLG,
	CURRENCY_ITL,
	CURRENCY_SEK,
	CURRENCY_RUR,
	CURRENCY_EUR,
};

/**
 * Will return the ottd's index correspondence to
 * the ttdpatch's id.  If the id is bigger than the array,
 * it is a grf written for ottd, thus returning the same id.
 * Only called from newgrf.cpp
 * @param grfcurr_id currency id coming from newgrf
 * @return the corrected index
 */
byte GetNewgrfCurrencyIdConverted(byte grfcurr_id)
{
	return (grfcurr_id >= lengthof(TTDPatch_To_OTTDIndex)) ? grfcurr_id : TTDPatch_To_OTTDIndex[grfcurr_id];
}

/**
 * get a mask of the allowed currencies depending on the year
 * @return mask of currencies
 */
uint64 GetMaskOfAllowedCurrencies()
{
	uint64 mask = 0LL;
	uint i;

	for (i = 0; i < CURRENCY_END; i++) {
		Year to_euro = _currency_specs[i].to_euro;

		if (to_euro != CF_NOEURO && to_euro != CF_ISEURO && _cur_year >= to_euro) continue;
		if (to_euro == CF_ISEURO && _cur_year < 2000) continue;
		SetBit(mask, i);
	}
	SetBit(mask, CURRENCY_CUSTOM); // always allow custom currency
	return mask;
}

/**
 * Verify if the currency chosen by the user is about to be converted to Euro
 */
void CheckSwitchToEuro()
{
	if (_currency_specs[_settings_game.locale.currency].to_euro != CF_NOEURO &&
			_currency_specs[_settings_game.locale.currency].to_euro != CF_ISEURO &&
			_cur_year >= _currency_specs[_settings_game.locale.currency].to_euro) {
		_settings_game.locale.currency = 2; // this is the index of euro above.
		AddNewsItem(STR_NEWS_EURO_INTRODUCTION, NT_ECONOMY, NF_NORMAL);
	}
}

/**
 * Will fill _currency_specs array with
 * default values from origin_currency_specs
 * Called only from newgrf.cpp and settings.cpp.
 * @param preserve_custom will not reset custom currency
 */
void ResetCurrencies(bool preserve_custom)
{
	for (uint i = 0; i < CURRENCY_END; i++) {
		if (preserve_custom && i == CURRENCY_CUSTOM) continue;
		_currency_specs[i] = origin_currency_specs[i];
	}
}

/**
 * Build a list of currency names StringIDs to use in a dropdown list
 * @return Pointer to a (static) array of StringIDs
 */
StringID *BuildCurrencyDropdown()
{
	/* Allow room for all currencies, plus a terminator entry */
	static StringID names[CURRENCY_END + 1];
	uint i;

	/* Add each name */
	for (i = 0; i < CURRENCY_END; i++) {
		names[i] = _currency_specs[i].name;
	}
	/* Terminate the list */
	names[i] = INVALID_STRING_ID;

	return names;
}
