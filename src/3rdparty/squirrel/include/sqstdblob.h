/*	see copyright notice in squirrel.h */
#ifndef _SQSTDBLOB_H_
#define _SQSTDBLOB_H_

SQUserPointer sqstd_createblob(HSQUIRRELVM v, SQInteger size);
SQRESULT sqstd_getblob(HSQUIRRELVM v,SQInteger idx,SQUserPointer *ptr);
SQInteger sqstd_getblobsize(HSQUIRRELVM v,SQInteger idx);

SQRESULT sqstd_register_bloblib(HSQUIRRELVM v);

#endif /*_SQSTDBLOB_H_*/

