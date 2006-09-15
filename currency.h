/* $Id$ */

#ifndef CURRENCY_H
#define CURRENCY_H

enum {
	CF_NOEURO = 0,
	CF_ISEURO = 1,
	NUM_CURRENCY = 26,
	CUSTOM_CURRENCY_ID = NUM_CURRENCY - 1
};

typedef struct {
	uint16 rate;
	char separator;
	Year to_euro;
	char prefix[16];
	char suffix[16];
	/**
	 * Position of the currency symbol on the amount string.
	 * 0 = placed before, 1 = placed after
	 */
	byte symbol_pos;
	StringID name;
} CurrencySpec;


extern CurrencySpec _currency_specs[NUM_CURRENCY];

// XXX small hack, but makes the rest of the code a bit nicer to read
#define _custom_currency (_currency_specs[CUSTOM_CURRENCY_ID])
#define _currency ((const CurrencySpec*)&_currency_specs[_opt_ptr->currency])

uint GetMaskOfAllowedCurrencies(void);
void CheckSwitchToEuro(void);
void ResetCurrencies(void);
StringID* BuildCurrencyDropdown(void);

#endif /* CURRENCY_H */
