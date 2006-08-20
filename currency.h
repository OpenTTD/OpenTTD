/* $Id$ */

#ifndef CURRENCY_H
#define CURRENCY_H

enum {
	CF_NOEURO = 0,
	CF_ISEURO = 1,
};

typedef struct {
	uint16 rate;
	char separator;
	Year to_euro;
	char prefix[16];
	char suffix[16];
} CurrencySpec;

extern CurrencySpec _currency_specs[];
extern const StringID _currency_string_list[];

// XXX small hack, but makes the rest of the code a bit nicer to read
#define CUSTOM_CURRENCY_ID 24
#define _custom_currency (_currency_specs[CUSTOM_CURRENCY_ID])
#define _currency ((const CurrencySpec*)&_currency_specs[_opt_ptr->currency])

uint GetMaskOfAllowedCurrencies(void);
void CheckSwitchToEuro(void);

#endif /* CURRENCY_H */
