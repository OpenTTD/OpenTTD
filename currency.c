/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "news.h"
#include "variables.h"
#include "table/strings.h"
#include "date.h"

	//   exchange rate    prefix             symbol_pos
	//   |  separator        |     postfix   |
	//   |   |    Euro year  |       |       |    name
	//   |   |    |          |       |       |    |
static const CurrencySpec origin_currency_specs[NUM_CURRENCY] = {
	{    1, ',', CF_NOEURO, "\xA3", "",      0,  STR_CURR_GBP    }, // british pounds
	{    2, ',', CF_NOEURO, "$",    "",      0,  STR_CURR_USD    }, // us dollars
	{    2, ',', CF_ISEURO, "¤",    "",      0,  STR_CURR_EUR    }, // Euro
	{  200, ',', CF_NOEURO, "\xA5", "",      0,  STR_CURR_YEN    }, // yen
	{   19, ',', 2002,      "",     " S.",   1,  STR_CURR_ATS    }, // austrian schilling
	{   57, ',', 2002,      "BEF ", "",      0,  STR_CURR_BEF    }, // belgian franc
	{    2, ',', CF_NOEURO, "CHF ", "",      0,  STR_CURR_CHF    }, // swiss franc
	{   50, ',', CF_NOEURO, "",     " Kc",   1,  STR_CURR_CZK    }, // czech koruna // TODO: Should use the "c" with an upside down "^"
	{    4, '.', 2002,      "DM ",  "",      0,  STR_CURR_DEM    }, // deutsche mark
	{   10, '.', CF_NOEURO, "",     " kr",   1,  STR_CURR_DKK    }, // danish krone
	{  200, '.', 2002,      "Pts ", "",      0,  STR_CURR_ESP    }, // spanish pesetas
	{    8, ',', 2002,      "",     " mk",   1,  STR_CURR_FIM    }, // finnish markka
	{   10, '.', 2002,      "FF ",  "",      0,  STR_CURR_FRF    }, // french francs
	{  480, ',', 2002,      "",     "Dr.",   1,  STR_CURR_GRD    }, // greek drachma
	{  376, ',', 2002,      "",     " Ft",   1,  STR_CURR_HUF    }, // hungarian forint
	{  130, '.', CF_NOEURO, "",     " Kr",   1,  STR_CURR_ISK    }, // icelandic krona
	{ 2730, ',', 2002,      "",     " L.",   1,  STR_CURR_ITL    }, // italian lira
	{    3, ',', 2002,      "NLG ", "",      0,  STR_CURR_NLG    }, // dutch gulden
	{   11, '.', CF_NOEURO, "",     " Kr",   1,  STR_CURR_NOK    }, // norwegian krone
	{    6, ' ', CF_NOEURO, "",     " zl",   1,  STR_CURR_PLN    }, // polish zloty
	{    6, '.', CF_NOEURO, "",     " Lei",  1,  STR_CURR_ROL    }, // romanian Lei
	{    5, ' ', CF_NOEURO, "",     " p",    1,  STR_CURR_RUR    }, // russian rouble
	{  350, '.', CF_NOEURO, "",     " SIT",  1,  STR_CURR_SIT    }, // slovenian tolar
	{   13, '.', CF_NOEURO, "",     " Kr",   1,  STR_CURR_SEK    }, // swedish krona
	{    2, '.', CF_NOEURO, "",     " YTL",  1,  STR_CURR_YTL    }, // turkish lira
	{   50, ',', CF_NOEURO, "",     " Sk",   1,  STR_CURR_SKK    }, // slovak koruna
	{    6, ',', CF_NOEURO, "R$ ",  "",      0,  STR_CURR_BRR    }, // brazil real
	{    1, ' ', CF_NOEURO, "",     "",      2,  STR_CURR_CUSTOM }, // custom currency
};

/* Array of currencies used by the system */
CurrencySpec _currency_specs[NUM_CURRENCY];

/**
 * These enums are only declared in order to make sens
 * out of the TTDPatch_To_OTTDIndex array that will follow
 * Every currency used by Ottd is there, just in case TTDPatch will
 * add those missing in its code
 **/
enum {
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
	CURR_ROL,
	CURR_RUR,
	CURR_SIT,
	CURR_SEK,
	CURR_YTL,
};

/**
 * This array represent the position of OpenTTD's currencies,
 * compared to TTDPatch's ones.
 * When a grf sends currencies, they are based on the order defined by TTDPatch.
 * So, we must reindex them to our own order.
 **/
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
 * the ttdpatch's id.  If the id is bigger then the array,
 * it is  a grf written for ottd, thus returning the same id.
 * Only called from newgrf.c
 * @param grfcurr_id currency id coming from newgrf
 * @return the corrected index
 **/
byte GetNewgrfCurrencyIdConverted(byte grfcurr_id)
{
	return (grfcurr_id >= lengthof(TTDPatch_To_OTTDIndex)) ? grfcurr_id : TTDPatch_To_OTTDIndex[grfcurr_id];
}

/* get a mask of the allowed currencies depending on the year */
uint GetMaskOfAllowedCurrencies(void)
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
 **/
void CheckSwitchToEuro(void)
{
	if (_currency_specs[_opt.currency].to_euro != CF_NOEURO &&
			_currency_specs[_opt.currency].to_euro != CF_ISEURO &&
			_cur_year >= _currency_specs[_opt.currency].to_euro) {
		_opt.currency = 2; // this is the index of euro above.
		AddNewsItem(STR_EURO_INTRODUCE, NEWS_FLAGS(NM_NORMAL, 0, NT_ECONOMY, 0), 0, 0);
	}
}

/**
 * Called only from newgrf.c.  Will fill _currency_specs array with
 * default values from origin_currency_specs
 **/
void ResetCurrencies(void)
{
	memcpy(&_currency_specs, &origin_currency_specs, sizeof(origin_currency_specs));
}

/**
 * Build a list of currency names StringIDs to use in a dropdown list
 * @return Pointer to a (static) array of StringIDs
 */
StringID* BuildCurrencyDropdown(void)
{
	/* Allow room for all currencies, plus a terminator entry */
	static StringID names[CUSTOM_CURRENCY_ID];
	uint i;

	/* Add each name */
	for (i = 0; i < NUM_CURRENCY; i++) {
		names[i] = _currency_specs[i].name;
	}
	/* Terminate the list */
	names[i] = INVALID_STRING_ID;

	return names;
}
