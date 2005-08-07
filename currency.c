/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "news.h"
#include "variables.h"
#include "table/strings.h"

// exchange rate    prefix
// |  separator        |     postfix
// |   |    Euro year  |       |
// |   |    |          |       |
CurrencySpec _currency_specs[] = {
	{    1, ',', CF_NOEURO, "\xA3", ""     }, // british pounds
	{    2, ',', CF_NOEURO, "$",    ""     }, // us dollars
	{    2, ',', CF_ISEURO, "¤",    ""     }, // Euro
	{  200, ',', CF_NOEURO, "\xA5", ""     }, // yen
	{   19, ',', 2002,      "",     " S."  }, // austrian schilling
	{   57, ',', 2002,      "BEF ", ""     }, // belgian franc
	{    2, ',', CF_NOEURO, "CHF ", ""     }, // swiss franc
	{   50, ',', CF_NOEURO, "",     " Kc"  }, // czech koruna // TODO: Should use the "c" with an upside down "^"
	{    4, '.', 2002,      "DM ",  ""     }, // deutsche mark
	{   10, '.', CF_NOEURO, "",     " kr"  }, // danish krone
	{  200, '.', 2002,      "Pts ", ""     }, // spanish pesetas
	{    8, ',', 2002,      "",     " MK"  }, // finnish markka
	{   10, '.', 2002,      "FF ",  ""     }, // french francs
	{  480, ',', 2002,      "",     "Dr."  }, // greek drachma
	{  376, ',', 2002,      "",     " Ft"  }, // hungarian forint
	{  130, '.', CF_NOEURO, "",     " Kr"  }, // icelandic krona
	{ 2730, ',', 2002,      "",     " L."  }, // italian lira
	{    3, ',', 2002,      "NLG ", ""     }, // dutch gulden
	{   11, '.', CF_NOEURO, "",     " Kr"  }, // norwegian krone
	{    6, ' ', CF_NOEURO, "",     " zl"  }, // polish zloty
	{    6, '.', CF_NOEURO, "",     " Lei" }, // romanian Lei
	{    5, ' ', CF_NOEURO, "",     " p"   }, // russian rouble
	{   13, '.', CF_NOEURO, "",     " Kr"  }, // swedish krona
	{    1, ' ', CF_NOEURO, "",     ""     }, // custom currency
};

const StringID _currency_string_list[] = {
	STR_CURR_GBP,
	STR_CURR_USD,
	STR_CURR_EUR,
	STR_CURR_YEN,
	STR_CURR_ATS,
	STR_CURR_BEF,
	STR_CURR_CHF,
	STR_CURR_CZK,
	STR_CURR_DEM,
	STR_CURR_DKK,
	STR_CURR_ESP,
	STR_CURR_FIM,
	STR_CURR_FRF,
	STR_CURR_GRD,
	STR_CURR_HUF,
	STR_CURR_ISK,
	STR_CURR_ITL,
	STR_CURR_NLG,
	STR_CURR_NOK,
	STR_CURR_PLN,
	STR_CURR_ROL,
	STR_CURR_RUR,
	STR_CURR_SEK,
	STR_CURR_CUSTOM,
	INVALID_STRING_ID
};

// NOTE: Make sure both lists are in the same order
// + 1 string list terminator
assert_compile(lengthof(_currency_specs) + 1 == lengthof(_currency_string_list));


// get a mask of the allowed currencies depending on the year
uint GetMaskOfAllowedCurrencies(void)
{
	uint mask = 0;
	uint i;

	for (i = 0; i != lengthof(_currency_specs); i++) {
		uint16 to_euro = _currency_specs[i].to_euro;

		if (to_euro != CF_NOEURO && to_euro != CF_ISEURO && _cur_year >= to_euro - MAX_YEAR_BEGIN_REAL) continue;
		if (to_euro == CF_ISEURO && _cur_year < 2000 - MAX_YEAR_BEGIN_REAL) continue;
		mask |= (1 << i);
	}
	mask |= (1 << 23); // always allow custom currency
	return mask;
}


uint GetCurrentCurrencyRate(void)
{
	return _currency_specs[_opt_ptr->currency].rate;
}


void CheckSwitchToEuro(void)
{
	if (_currency_specs[_opt.currency].to_euro != CF_NOEURO &&
			_currency_specs[_opt.currency].to_euro != CF_ISEURO &&
			MAX_YEAR_BEGIN_REAL + _cur_year >= _currency_specs[_opt.currency].to_euro) {
		_opt.currency = 2; // this is the index of euro above.
		AddNewsItem(STR_EURO_INTRODUCE, NEWS_FLAGS(NM_NORMAL, 0, NT_ECONOMY, 0), 0, 0);
	}
}
