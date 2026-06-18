/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file currency_type.h Types related to currencies. */

#ifndef CURRENCY_TYPE_H
#define CURRENCY_TYPE_H

#include "timer/timer_game_calendar.h"
#include "strings_type.h"

static constexpr TimerGameCalendar::Year CF_NOEURO{0}; ///< Currency never switches to the Euro (as far as known).
static constexpr TimerGameCalendar::Year CF_ISEURO{1}; ///< Currency _is_ the Euro.
static constexpr TimerGameCalendar::Year MIN_EURO_YEAR{2000}; ///< The earliest year custom currencies may switch to the Euro.

/**
 * This enum gives the currencies a unique id which must be maintained for
 * savegame compatibility and in order to refer to them quickly, especially
 * for referencing the custom one.
 */
enum class Currency : uint8_t {
	GBP, ///< British Pound
	USD, ///< US Dollar
	EUR, ///< Euro
	JPY, ///< Japanese Yen
	ATS, ///< Austrian Schilling
	BEF, ///< Belgian Franc
	CHF, ///< Swiss Franc
	CZK, ///< Czech Koruna
	DEM, ///< Deutsche Mark
	DKK, ///< Danish Krona
	ESP, ///< Spanish Peseta
	FIM, ///< Finish Markka
	FRF, ///< French Franc
	GRD, ///< Greek Drachma
	HUF, ///< Hungarian Forint
	ISK, ///< Icelandic Krona
	ITL, ///< Italian Lira
	NLG, ///< Dutch Gulden
	NOK, ///< Norwegian Krone
	PLN, ///< Polish Zloty
	RON, ///< Romanian Leu
	RUR, ///< Russian Rouble
	SIT, ///< Slovenian Tolar
	SEK, ///< Swedish Krona
	YTL, ///< Turkish Lira
	SKK, ///< Slovak Kornuna
	BRL, ///< Brazilian Real
	EEK, ///< Estonian Krooni
	LTL, ///< Lithuanian Litas
	KRW, ///< South Korean Won
	ZAR, ///< South African Rand
	Custom, ///< Custom currency
	GEL, ///< Georgian Lari
	IRR, ///< Iranian Rial
	RUB, ///< New Russian Ruble
	MXN, ///< Mexican Peso
	NTD, ///< New Taiwan Dollar
	CNY, ///< Chinese Renminbi
	HKD, ///< Hong Kong Dollar
	INR, ///< Indian Rupee
	IDR, ///< Indonesian Rupiah
	MYR, ///< Malaysian Ringgit
	LVL, ///< Latvian Lats
	PTE, ///< Portuguese Escudo
	UAH, ///< Ukrainian Hryvnia
	VND, ///< Vietnamese Dong

	End, ///< Always the last item.
};

/** Bitmask of \c Currency. */
using Currencies = EnumBitSet<Currency, uint64_t, Currency::End>;

/** The currency symbol positions that we can show. */
enum class CurrencySymbolPosition : uint8_t {
	Prefix, ///< Show the prefix value.
	Suffix, ///< Show the suffix value.
};

/** Bitmask of \c CurrencySymbolPosition. */
using CurrencySymbolPositions = EnumBitSet<CurrencySymbolPosition, uint8_t>;

/** Specification of a currency. */
struct CurrencySpec {
	uint16_t rate; ///< The conversion rate compared to the base currency.
	std::string separator; ///< The thousands separator for this currency.
	TimerGameCalendar::Year to_euro; ///< Year of switching to the Euro. May also be #CF_NOEURO or #CF_ISEURO.
	std::string prefix; ///< Prefix to apply when formatting money in this currency.
	std::string suffix; ///< Suffix to apply when formatting money in this currency.
	std::string code; ///< 3 letter untranslated code to identify the currency.
	CurrencySymbolPositions symbol_pos; ///< Which currency symbols should we show?
	StringID name; ///< Translated name of this currency.
};

#endif /* CURRENCY_TYPE_H */
