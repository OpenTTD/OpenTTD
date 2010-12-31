/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file currency.cpp Support for different currencies. */

#include "stdafx.h"
#include "currency.h"
#include "news_func.h"
#include "settings_type.h"
#include "date_func.h"
#include "string_type.h"

#include "table/strings.h"

	/*   exchange rate    prefix                         symbol_pos
	 *   |  separator        |           postfix             |
	 *   |   |   Euro year   |              |                | name
	 *   |   |    |          |              |                |  | */
static const CurrencySpec origin_currency_specs[NUM_CURRENCY] = {
	{    1, "", CF_NOEURO, "\xC2\xA3",     "",              0, STR_GAME_OPTIONS_CURRENCY_GBP    }, ///< british pounds
	{    2, "", CF_NOEURO, "$",            "",              0, STR_GAME_OPTIONS_CURRENCY_USD    }, ///< us dollars
	{    2, "", CF_ISEURO, "\xE2\x82\xAC", "",              0, STR_GAME_OPTIONS_CURRENCY_EUR    }, ///< Euro
	{  220, "", CF_NOEURO, "\xC2\xA5",     "",              0, STR_GAME_OPTIONS_CURRENCY_YEN    }, ///< yen
	{   20, "", 2002,      "",             NBSP"S.",        1, STR_GAME_OPTIONS_CURRENCY_ATS    }, ///< austrian schilling
	{   59, "", 2002,      "BEF"NBSP,      "",              0, STR_GAME_OPTIONS_CURRENCY_BEF    }, ///< belgian franc
	{    2, "", CF_NOEURO, "CHF"NBSP,      "",              0, STR_GAME_OPTIONS_CURRENCY_CHF    }, ///< swiss franc
	{   41, "", CF_NOEURO, "",             NBSP"K\xC4\x8D", 1, STR_GAME_OPTIONS_CURRENCY_CZK    }, ///< czech koruna
	{    3, "", 2002,      "DM"NBSP,       "",              0, STR_GAME_OPTIONS_CURRENCY_DEM    }, ///< deutsche mark
	{   11, "", CF_NOEURO, "",             NBSP"kr",        1, STR_GAME_OPTIONS_CURRENCY_DKK    }, ///< danish krone
	{  245, "", 2002,      "Pts"NBSP,      "",              0, STR_GAME_OPTIONS_CURRENCY_ESP    }, ///< spanish pesetas
	{    9, "", 2002,      "",             NBSP"mk",        1, STR_GAME_OPTIONS_CURRENCY_FIM    }, ///< finnish markka
	{   10, "", 2002,      "FF"NBSP,       "",              0, STR_GAME_OPTIONS_CURRENCY_FRF    }, ///< french francs
	{  500, "", 2002,      "",             "Dr.",           1, STR_GAME_OPTIONS_CURRENCY_GRD    }, ///< greek drachma
	{  378, "", CF_NOEURO, "",             NBSP"Ft",        1, STR_GAME_OPTIONS_CURRENCY_HUF    }, ///< hungarian forint
	{  130, "", CF_NOEURO, "",             NBSP"Kr",        1, STR_GAME_OPTIONS_CURRENCY_ISK    }, ///< icelandic krona
	{ 2850, "", 2002,      "",             NBSP"L.",        1, STR_GAME_OPTIONS_CURRENCY_ITL    }, ///< italian lira
	{    3, "", 2002,      "NLG"NBSP,      "",              0, STR_GAME_OPTIONS_CURRENCY_NLG    }, ///< dutch gulden
	{   12, "", CF_NOEURO, "",             NBSP"Kr",        1, STR_GAME_OPTIONS_CURRENCY_NOK    }, ///< norwegian krone
	{    6, "", CF_NOEURO, "",             NBSP"z\xC5\x82", 1, STR_GAME_OPTIONS_CURRENCY_PLN    }, ///< polish zloty
	{    5, "", CF_NOEURO, "",             NBSP"Lei",       1, STR_GAME_OPTIONS_CURRENCY_RON    }, ///< romanian Lei
	{   50, "", CF_NOEURO, "",             NBSP"p",         1, STR_GAME_OPTIONS_CURRENCY_RUR    }, ///< russian rouble
	{  352, "", 2007,      "",             NBSP"SIT",       1, STR_GAME_OPTIONS_CURRENCY_SIT    }, ///< slovenian tolar
	{   13, "", CF_NOEURO, "",             NBSP"Kr",        1, STR_GAME_OPTIONS_CURRENCY_SEK    }, ///< swedish krona
	{    3, "", CF_NOEURO, "",             NBSP"TL",        1, STR_GAME_OPTIONS_CURRENCY_TRY    }, ///< turkish lira
	{   52, "", 2009,      "",             NBSP"Sk",        1, STR_GAME_OPTIONS_CURRENCY_SKK    }, ///< slovak koruna
	{    4, "", CF_NOEURO, "R$"NBSP,       "",              0, STR_GAME_OPTIONS_CURRENCY_BRL    }, ///< brazil real
	{   20, "", 2011,      "",             NBSP"EEK",       1, STR_GAME_OPTIONS_CURRENCY_EEK    }, ///< estonian krooni
	{    1, "", CF_NOEURO, "",             "",              2, STR_GAME_OPTIONS_CURRENCY_CUSTOM }, ///< custom currency
};

/* Array of currencies used by the system */
CurrencySpec _currency_specs[NUM_CURRENCY];

/**
 * These enums are only declared in order to make sens
 * out of the TTDPatch_To_OTTDIndex array that will follow
 * Every currency used by Ottd is there, just in case TTDPatch will
 * add those missing in its code
 */
enum Currencies {
	CURR_GBP,
	CURR_USD,
	CURR_EUR,
	CURR_YEN,
	CURR_ATS,
	CURR_BEF,
	CURR_CHF,
	CURR_CZK,
	CURR_DEM,
	CURR_DKK,
	CURR_ESP,
	CURR_FIM,
	CURR_FRF,
	CURR_GRD,
	CURR_HUF,
	CURR_ISK,
	CURR_ITL,
	CURR_NLG,
	CURR_NOK,
	CURR_PLN,
	CURR_RON,
	CURR_RUR,
	CURR_SIT,
	CURR_SEK,
	CURR_YTL,
	CURR_SKK,
	CURR_BRL,
	CURR_EEK,
};

/**
 * This array represent the position of OpenTTD's currencies,
 * compared to TTDPatch's ones.
 * When a grf sends currencies, they are based on the order defined by TTDPatch.
 * So, we must reindex them to our own order.
 */
const byte TTDPatch_To_OTTDIndex[] =
{
	CURR_GBP,
	CURR_USD,
	CURR_FRF,
	CURR_DEM,
	CURR_YEN,
	CURR_ESP,
	CURR_HUF,
	CURR_PLN,
	CURR_ATS,
	CURR_BEF,
	CURR_DKK,
	CURR_FIM,
	CURR_GRD,
	CURR_CHF,
	CURR_NLG,
	CURR_ITL,
	CURR_SEK,
	CURR_RUR,
	CURR_EUR,
};

/**
 * Will return the ottd's index correspondance to
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
uint GetMaskOfAllowedCurrencies()
{
	uint mask = 0;
	uint i;

	for (i = 0; i < NUM_CURRENCY; i++) {
		Year to_euro = _currency_specs[i].to_euro;

		if (to_euro != CF_NOEURO && to_euro != CF_ISEURO && _cur_year >= to_euro) continue;
		if (to_euro == CF_ISEURO && _cur_year < 2000) continue;
		mask |= (1 << i);
	}
	mask |= (1 << CUSTOM_CURRENCY_ID); // always allow custom currency
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
		AddNewsItem(STR_NEWS_EURO_INTRODUCTION, NS_ECONOMY);
	}
}

/**
 * Will fill _currency_specs array with
 * default values from origin_currency_specs
 * Called only from newgrf.cpp and settings.cpp.
 * @param preserve_custom will not reset custom currency (the latest one on the list)
 *        if ever it is flagged to true. In which case, the total size of the memory to move
 *        will be one currency spec less, thus preserving the custom curreny from been
 *        overwritten.
 */
void ResetCurrencies(bool preserve_custom)
{
	memcpy(&_currency_specs, &origin_currency_specs, sizeof(origin_currency_specs) - (preserve_custom ? sizeof(_custom_currency) : 0));
}

/**
 * Build a list of currency names StringIDs to use in a dropdown list
 * @return Pointer to a (static) array of StringIDs
 */
StringID *BuildCurrencyDropdown()
{
	/* Allow room for all currencies, plus a terminator entry */
	static StringID names[NUM_CURRENCY + 1];
	uint i;

	/* Add each name */
	for (i = 0; i < NUM_CURRENCY; i++) {
		names[i] = _currency_specs[i].name;
	}
	/* Terminate the list */
	names[i] = INVALID_STRING_ID;

	return names;
}
