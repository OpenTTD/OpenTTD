#ifndef STRINGS_H
#define STRINGS_H

byte *GetString(byte *buffr, uint16 string);

void InjectDParam(int amount);

int32 GetParamInt32(void);
int GetParamInt16(void);
int GetParamInt8(void);
int GetParamUint16(void);

uint GetCurrentCurrencyRate(void);

#endif
