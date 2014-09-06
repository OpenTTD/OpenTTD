/*	see copyright notice in squirrel.h */
#ifndef _SQLEXER_H_
#define _SQLEXER_H_

typedef unsigned short LexChar;

struct SQLexer
{
	SQLexer();
	~SQLexer();
	void Init(SQSharedState *ss,SQLEXREADFUNC rg,SQUserPointer up,CompilerErrorFunc efunc,void *ed);
	void Error(const SQChar *err);
	SQInteger Lex();
	const SQChar *Tok2Str(SQInteger tok);
private:
	SQInteger GetIDType(SQChar *s);
	SQInteger ReadString(LexChar ndelim,bool verbatim);
	SQInteger ReadNumber();
	void LexBlockComment();
	SQInteger ReadID();
	void Next();
	SQInteger _curtoken;
	SQTable *_keywords;
	void INIT_TEMP_STRING() { _longstr.resize(0); }
	void APPEND_CHAR(LexChar c);
	void TERMINATE_BUFFER() { _longstr.push_back('\0'); }

public:
	SQInteger _prevtoken;
	SQInteger _currentline;
	SQInteger _lasttokenline;
	SQInteger _currentcolumn;
	const SQChar *_svalue;
	SQInteger _nvalue;
	SQFloat _fvalue;
	SQLEXREADFUNC _readf;
	SQUserPointer _up;
	LexChar _currdata;
	SQSharedState *_sharedstate;
	sqvector<SQChar> _longstr;
	CompilerErrorFunc _errfunc;
	void *_errtarget;
};

#endif
