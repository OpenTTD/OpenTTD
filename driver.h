#ifndef DRIVER_H
#define DRIVER_H

void LoadDriver(int driver, const char *name);

bool GetDriverParamBool(const char* const* parm, const char* name);
int GetDriverParamInt(const char* const* parm, const char* name, int def);

void GetDriverList(char* p);

#endif
