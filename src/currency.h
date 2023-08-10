/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file currency.h Functions to handle different currencies. */

#ifndef CURRENCY_H
#define CURRENCY_H

#include "timer/timer_game_calendar.h"
#include "string_func.h"
#include "strings_type.h"

static constexpr TimerGameCalendar::Year CF_NOEURO = 0; ///< Currency never switches to the Euro (as far as known).
static constexpr TimerGameCalendar::Year CF_ISEURO = 1; ///< Currency _is_ the Euro.
static constexpr TimerGameCalendar::Year MIN_EURO_YEAR = 2000; ///< The earliest year custom currencies may switch to the Euro.

/**
 * This enum gives the currencies a unique id which must be maintained for
 * savegame compatibility and in order to refer to them quickly, especially
 * for referencing the custom one.
 */
enum Currencies {
	CURRENCY_GBP,       ///< British Pound
	CURRENCY_USD,       ///< US Dollar
	CURRENCY_EUR,       ///< Euro
	CURRENCY_JPY,       ///< Japanese Yen
	CURRENCY_ATS,       ///< Austrian Schilling
	CURRENCY_BEF,       ///< Belgian Franc
	CURRENCY_CHF,       ///< Swiss Franc
	CURRENCY_CZK,       ///< Czech Koruna
	CURRENCY_DEM,       ///< Deutsche Mark
	CURRENCY_DKK,       ///< Danish Krona
	CURRENCY_ESP,       ///< Spanish Peseta
	CURRENCY_FIM,       ///< Finish Markka
	CURRENCY_FRF,       ///< French Franc
	CURRENCY_GRD,       ///< Greek Drachma
	CURRENCY_HUF,       ///< Hungarian Forint
	CURRENCY_ISK,       ///< Icelandic Krona
	CURRENCY_ITL,       ///< Italian Lira
	CURRENCY_NLG,       ///< Dutch Gulden
	CURRENCY_NOK,       ///< Norwegian Krone
	CURRENCY_PLN,       ///< Polish Zloty
	CURRENCY_RON,       ///< Romenian Leu
	CURRENCY_RUR,       ///< Russian Rouble
	CURRENCY_SIT,       ///< Slovenian Tolar
	CURRENCY_SEK,       ///< Swedish Krona
	CURRENCY_YTL,       ///< Turkish Lira
	CURRENCY_SKK,       ///< Slovak Kornuna
	CURRENCY_BRL,       ///< Brazilian Real
	CURRENCY_EEK,       ///< Estonian Krooni
	CURRENCY_LTL,       ///< Lithuanian Litas
	CURRENCY_KRW,       ///< South Korean Won
	CURRENCY_ZAR,       ///< South African Rand
	CURRENCY_CUSTOM,    ///< Custom currency
	CURRENCY_GEL,       ///< Georgian Lari
	CURRENCY_IRR,       ///< Iranian Rial
	CURRENCY_RUB,       ///< New Russian Ruble
	CURRENCY_MXN,       ///< Mexican Peso
	CURRENCY_NTD,       ///< New Taiwan Dollar
	CURRENCY_CNY,       ///< Chinese Renminbi
	CURRENCY_HKD,       ///< Hong Kong Dollar
	CURRENCY_INR,       ///< Indian Rupee
	CURRENCY_IDR,       ///< Indonesian Rupiah
	CURRENCY_MYR,       ///< Malaysian Ringgit
	CURRENCY_END,       ///< always the last item
};

/** Specification of a currency. */
struct CurrencySpec {
	uint16_t rate;           ///< The conversion rate compared to the base currency.
	std::string separator; ///< The thousands separator for this currency.
	TimerGameCalendar::Year to_euro; ///< Year of switching to the Euro. May also be #CF_NOEURO or #CF_ISEURO.
	std::string prefix;    ///< Prefix to apply when formatting money in this currency.
	std::string suffix;    ///< Suffix to apply when formatting money in this currency.
	std::string code; ///< 3 letter untranslated code to identify the currency.
	/**
	 * The currency symbol is represented by two possible values, prefix and suffix
	 * Usage of one or the other is determined by #symbol_pos.
	 * 0 = prefix
	 * 1 = suffix
	 * 2 = both : Special case only for custom currency.
	 *            It is not a spec from Newgrf,
	 *            rather a way to let users do what they want with custom currency
	 */
	byte symbol_pos;
	StringID name;

	CurrencySpec() = default;

	CurrencySpec(uint16_t rate, const char *separator, TimerGameCalendar::Year to_euro, const char *prefix, const char *suffix, const char *code, byte symbol_pos, StringID name) :
		rate(rate), separator(separator), to_euro(to_euro), prefix(prefix), suffix(suffix), code(code), symbol_pos(symbol_pos), name(name)
	{
	}
};

extern CurrencySpec _currency_specs[CURRENCY_END];

/* XXX small hack, but makes the rest of the code a bit nicer to read */
#define _custom_currency (_currency_specs[CURRENCY_CUSTOM])
#define _currency ((const CurrencySpec*)&_currency_specs[GetGameSettings().locale.currency])

uint64_t GetMaskOfAllowedCurrencies();
void ResetCurrencies(bool preserve_custom = true);
byte GetNewgrfCurrencyIdConverted(byte grfcurr_id);

#endif /* CURRENCY_H */
