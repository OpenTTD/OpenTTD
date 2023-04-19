/*	see copyright notice in squirrel.h */
#ifndef _SQSTD_STRING_H_
#define _SQSTD_STRING_H_

typedef unsigned int SQRexBool;
typedef struct SQRex SQRex;

typedef struct {
	const SQChar *begin;
	SQInteger len;
} SQRexMatch;

SQRex *sqstd_rex_compile(const SQChar *pattern,const SQChar **error);
void sqstd_rex_free(SQRex *exp);
SQBool sqstd_rex_match(SQRex* exp,const SQChar* text);
SQBool sqstd_rex_search(SQRex* exp,const SQChar* text, const SQChar** out_begin, const SQChar** out_end);
SQBool sqstd_rex_searchrange(SQRex* exp,const SQChar* text_begin,const SQChar* text_end,const SQChar** out_begin, const SQChar** out_end);
SQInteger sqstd_rex_getsubexpcount(SQRex* exp);
SQBool sqstd_rex_getsubexp(SQRex* exp, SQInteger n, SQRexMatch *subexp);

SQRESULT sqstd_register_stringlib(HSQUIRRELVM v);

#endif /*_SQSTD_STRING_H_*/
