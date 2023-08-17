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
#include "string_type.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"

#include "table/strings.h"

#include "safeguards.h"

	/*   exchange rate    prefix                             code
	 *   |  separator        |           postfix              |  symbol_pos
	 *   |   |   Euro year   |              |                 |     | name
	 *   |   |    |          |              |                 |     |  | */
/** The original currency specifications. */
static const CurrencySpec origin_currency_specs[CURRENCY_END] = {
	{    1, "", CF_NOEURO, u8"\u00a3",     "",               "GBP", 0, STR_GAME_OPTIONS_CURRENCY_GBP    }, ///< british pound
	{    2, "", CF_NOEURO, "$",            "",               "USD", 0, STR_GAME_OPTIONS_CURRENCY_USD    }, ///< american dollar
	{    2, "", CF_ISEURO, u8"\u20ac",     "",               "EUR", 0, STR_GAME_OPTIONS_CURRENCY_EUR    }, ///< euro
	{  220, "", CF_NOEURO, u8"\u00a5",     "",               "JPY", 0, STR_GAME_OPTIONS_CURRENCY_JPY    }, ///< japanese yen
	{   27, "", 2002,      "",             NBSP "S.",        "ATS", 1, STR_GAME_OPTIONS_CURRENCY_ATS    }, ///< austrian schilling
	{   81, "", 2002,      "BEF" NBSP,     "",               "BEF", 0, STR_GAME_OPTIONS_CURRENCY_BEF    }, ///< belgian franc
	{    2, "", CF_NOEURO, "CHF" NBSP,     "",               "CHF", 0, STR_GAME_OPTIONS_CURRENCY_CHF    }, ///< swiss franc
	{   41, "", CF_NOEURO, "",             NBSP u8"K\u010d", "CZK", 1, STR_GAME_OPTIONS_CURRENCY_CZK    }, ///< czech koruna
	{    4, "", 2002,      "DM" NBSP,      "",               "DEM", 0, STR_GAME_OPTIONS_CURRENCY_DEM    }, ///< deutsche mark
	{   11, "", CF_NOEURO, "",             NBSP "kr",        "DKK", 1, STR_GAME_OPTIONS_CURRENCY_DKK    }, ///< danish krone
	{  333, "", 2002,      "Pts" NBSP,     "",               "ESP", 0, STR_GAME_OPTIONS_CURRENCY_ESP    }, ///< spanish peseta
	{   12, "", 2002,      "",             NBSP "mk",        "FIM", 1, STR_GAME_OPTIONS_CURRENCY_FIM    }, ///< finnish markka
	{   13, "", 2002,      "FF" NBSP,      "",               "FRF", 0, STR_GAME_OPTIONS_CURRENCY_FRF    }, ///< french franc
	{  681, "", 2002,      "",             "Dr.",            "GRD", 1, STR_GAME_OPTIONS_CURRENCY_GRD    }, ///< greek drachma
	{  378, "", CF_NOEURO, "",             NBSP "Ft",        "HUF", 1, STR_GAME_OPTIONS_CURRENCY_HUF    }, ///< hungarian forint
	{  130, "", CF_NOEURO, "",             NBSP "Kr",        "ISK", 1, STR_GAME_OPTIONS_CURRENCY_ISK    }, ///< icelandic krona
	{ 3873, "", 2002,      "",             NBSP "L.",        "ITL", 1, STR_GAME_OPTIONS_CURRENCY_ITL    }, ///< italian lira
	{    4, "", 2002,      "NLG" NBSP,     "",               "NLG", 0, STR_GAME_OPTIONS_CURRENCY_NLG    }, ///< dutch gulden
	{   12, "", CF_NOEURO, "",             NBSP "Kr",        "NOK", 1, STR_GAME_OPTIONS_CURRENCY_NOK    }, ///< norwegian krone
	{    6, "", CF_NOEURO, "",             NBSP u8"z\u0142", "PLN", 1, STR_GAME_OPTIONS_CURRENCY_PLN    }, ///< polish zloty
	{    5, "", CF_NOEURO, "",             NBSP "Lei",       "RON", 1, STR_GAME_OPTIONS_CURRENCY_RON    }, ///< romanian leu
	{   50, "", CF_NOEURO, "",             NBSP "p",         "RUR", 1, STR_GAME_OPTIONS_CURRENCY_RUR    }, ///< russian rouble
	{  479, "", 2007,      "",             NBSP "SIT",       "SIT", 1, STR_GAME_OPTIONS_CURRENCY_SIT    }, ///< slovenian tolar
	{   13, "", CF_NOEURO, "",             NBSP "Kr",        "SEK", 1, STR_GAME_OPTIONS_CURRENCY_SEK    }, ///< swedish krona
	{    3, "", CF_NOEURO, "",             NBSP "TL",        "TRY", 1, STR_GAME_OPTIONS_CURRENCY_TRY    }, ///< turkish lira
	{   60, "", 2009,      "",             NBSP "Sk",        "SKK", 1, STR_GAME_OPTIONS_CURRENCY_SKK    }, ///< slovak koruna
	{    4, "", CF_NOEURO, "R$" NBSP,      "",               "BRL", 0, STR_GAME_OPTIONS_CURRENCY_BRL    }, ///< brazil real
	{   31, "", 2011,      "",             NBSP "EEK",       "EEK", 1, STR_GAME_OPTIONS_CURRENCY_EEK    }, ///< estonian krooni
	{    4, "", 2015,      "",             NBSP "Lt",        "LTL", 1, STR_GAME_OPTIONS_CURRENCY_LTL    }, ///< lithuanian litas
	{ 1850, "", CF_NOEURO, u8"\u20a9",     "",               "KRW", 0, STR_GAME_OPTIONS_CURRENCY_KRW    }, ///< south korean won
	{   13, "", CF_NOEURO, "R" NBSP,       "",               "ZAR", 0, STR_GAME_OPTIONS_CURRENCY_ZAR    }, ///< south african rand
	{    1, "", CF_NOEURO, "",             "",               "",    2, STR_GAME_OPTIONS_CURRENCY_CUSTOM }, ///< custom currency (add further languages below)
	{    3, "", CF_NOEURO, "",             NBSP "GEL",       "GEL", 1, STR_GAME_OPTIONS_CURRENCY_GEL    }, ///< Georgian Lari
	{ 4901, "", CF_NOEURO, "",             NBSP "Rls",       "IRR", 1, STR_GAME_OPTIONS_CURRENCY_IRR    }, ///< Iranian Rial
	{   80, "", CF_NOEURO, "",             NBSP "rub",       "RUB", 1, STR_GAME_OPTIONS_CURRENCY_RUB    }, ///< New Russian Ruble
	{   24, "", CF_NOEURO, "$",            "",               "MXN", 0, STR_GAME_OPTIONS_CURRENCY_MXN    }, ///< Mexican peso
	{   40, "", CF_NOEURO, "NTD" NBSP,     "",               "NTD", 0, STR_GAME_OPTIONS_CURRENCY_NTD    }, ///< new taiwan dollar
	{    8, "", CF_NOEURO, u8"\u00a5",     "",               "CNY", 0, STR_GAME_OPTIONS_CURRENCY_CNY    }, ///< chinese renminbi
	{   10, "", CF_NOEURO, "HKD" NBSP,     "",               "HKD", 0, STR_GAME_OPTIONS_CURRENCY_HKD    }, ///< hong kong dollar
	{   90, "", CF_NOEURO, u8"\u20b9",     "",               "INR", 0, STR_GAME_OPTIONS_CURRENCY_INR    }, ///< Indian Rupee
	{   19, "", CF_NOEURO, "Rp",           "",               "IDR", 0, STR_GAME_OPTIONS_CURRENCY_IDR    }, ///< Indonesian Rupiah
	{    5, "", CF_NOEURO, "RM",           "",               "MYR", 0, STR_GAME_OPTIONS_CURRENCY_MYR    }, ///< Malaysian Ringgit
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
uint64_t GetMaskOfAllowedCurrencies()
{
	uint64_t mask = 0LL;
	uint i;

	for (i = 0; i < CURRENCY_END; i++) {
		TimerGameCalendar::Year to_euro = _currency_specs[i].to_euro;

		if (to_euro != CF_NOEURO && to_euro != CF_ISEURO && TimerGameCalendar::year >= to_euro) continue;
		if (to_euro == CF_ISEURO && TimerGameCalendar::year < 2000) continue;
		SetBit(mask, i);
	}
	SetBit(mask, CURRENCY_CUSTOM); // always allow custom currency
	return mask;
}

/**
 * Verify if the currency chosen by the user is about to be converted to Euro
 */
static IntervalTimer<TimerGameCalendar> _check_switch_to_euro({TimerGameCalendar::YEAR, TimerGameCalendar::Priority::NONE}, [](auto)
{
	if (_currency_specs[_settings_game.locale.currency].to_euro != CF_NOEURO &&
			_currency_specs[_settings_game.locale.currency].to_euro != CF_ISEURO &&
			TimerGameCalendar::year >= _currency_specs[_settings_game.locale.currency].to_euro) {
		_settings_game.locale.currency = 2; // this is the index of euro above.
		AddNewsItem(STR_NEWS_EURO_INTRODUCTION, NT_ECONOMY, NF_NORMAL);
	}
});

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
