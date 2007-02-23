/* $Id$ */

/** @file driver.h */

#ifndef DRIVER_H
#define DRIVER_H

void LoadDriver(int driver, const char *name);

bool GetDriverParamBool(const char* const* parm, const char* name);
int GetDriverParamInt(const char* const* parm, const char* name, int def);

char *GetDriverList(char *p, const char *last);

#endif /* DRIVER_H */
