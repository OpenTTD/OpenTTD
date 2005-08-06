#ifndef CURRENCY_H
#define CURRENCY_H

enum {
	CF_NOEURO = 0,
	CF_ISEURO = 1,
};

typedef struct {
	uint16 rate;
	char separator;
	uint16 to_euro;
	char prefix[16];
	char suffix[16];
} CurrencySpec;

extern CurrencySpec _currency_specs[];
extern const StringID _currency_string_list[];

uint GetMaskOfAllowedCurrencies(void);
uint GetCurrentCurrencyRate(void);

#endif
