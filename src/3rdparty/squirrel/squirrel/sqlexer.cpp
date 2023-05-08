/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"

#include "sqpcheader.h"
#include <ctype.h>
#include "sqtable.h"
#include "sqstring.h"
#include "sqcompiler.h"
#include "sqlexer.h"

#include "../../../string_func.h"

#include "../../../safeguards.h"

#define CUR_CHAR (_currdata)
#define RETURN_TOKEN(t) { _prevtoken = _curtoken; _curtoken = t; return t;}
#define IS_EOB() (CUR_CHAR <= SQUIRREL_EOB)
#define NEXT() {Next();_currentcolumn++;}
#define ADD_KEYWORD(key,id) _keywords->NewSlot( SQString::Create(ss, #key) ,SQInteger(id))

SQLexer::~SQLexer()
{
	_keywords->Release();
}

void SQLexer::APPEND_CHAR(char32_t c)
{
	char buf[4];
	size_t chars = Utf8Encode(buf, c);
	for (size_t i = 0; i < chars; i++) {
		_longstr.push_back(buf[i]);
	}
}

SQLexer::SQLexer(SQSharedState *ss, SQLEXREADFUNC rg, SQUserPointer up,CompilerErrorFunc efunc,void *ed)
{
	_errfunc = efunc;
	_errtarget = ed;
	_sharedstate = ss;
	_keywords = SQTable::Create(ss, 26);
	ADD_KEYWORD(while, TK_WHILE);
	ADD_KEYWORD(do, TK_DO);
	ADD_KEYWORD(if, TK_IF);
	ADD_KEYWORD(else, TK_ELSE);
	ADD_KEYWORD(break, TK_BREAK);
	ADD_KEYWORD(continue, TK_CONTINUE);
	ADD_KEYWORD(return, TK_RETURN);
	ADD_KEYWORD(null, TK_NULL);
	ADD_KEYWORD(function, TK_FUNCTION);
	ADD_KEYWORD(local, TK_LOCAL);
	ADD_KEYWORD(for, TK_FOR);
	ADD_KEYWORD(foreach, TK_FOREACH);
	ADD_KEYWORD(in, TK_IN);
	ADD_KEYWORD(typeof, TK_TYPEOF);
	ADD_KEYWORD(delegate, TK_DELEGATE);
	ADD_KEYWORD(delete, TK_DELETE);
	ADD_KEYWORD(try, TK_TRY);
	ADD_KEYWORD(catch, TK_CATCH);
	ADD_KEYWORD(throw, TK_THROW);
	ADD_KEYWORD(clone, TK_CLONE);
	ADD_KEYWORD(yield, TK_YIELD);
	ADD_KEYWORD(resume, TK_RESUME);
	ADD_KEYWORD(switch, TK_SWITCH);
	ADD_KEYWORD(case, TK_CASE);
	ADD_KEYWORD(default, TK_DEFAULT);
	ADD_KEYWORD(this, TK_THIS);
	ADD_KEYWORD(parent,TK_PARENT);
	ADD_KEYWORD(class,TK_CLASS);
	ADD_KEYWORD(extends,TK_EXTENDS);
	ADD_KEYWORD(constructor,TK_CONSTRUCTOR);
	ADD_KEYWORD(instanceof,TK_INSTANCEOF);
	ADD_KEYWORD(vargc,TK_VARGC);
	ADD_KEYWORD(vargv,TK_VARGV);
	ADD_KEYWORD(true,TK_TRUE);
	ADD_KEYWORD(false,TK_FALSE);
	ADD_KEYWORD(static,TK_STATIC);
	ADD_KEYWORD(enum,TK_ENUM);
	ADD_KEYWORD(const,TK_CONST);

	_readf = rg;
	_up = up;
	_lasttokenline = _currentline = 1;
	_currentcolumn = 0;
	_prevtoken = -1;
	_curtoken = -1;

	_svalue = nullptr;
	_nvalue = 0;
	_fvalue = 0;

	Next();
}

NORETURN void SQLexer::Error(const SQChar *err)
{
	_errfunc(_errtarget,err);
}

void SQLexer::Next()
{
	char32_t t = _readf(_up);
	if(t > MAX_CHAR) Error("Invalid character");
	if(t != 0) {
		_currdata = t;
		return;
	}
	_currdata = SQUIRREL_EOB;
}

const SQChar *SQLexer::Tok2Str(SQInteger tok)
{
	SQObjectPtr itr, key, val;
	SQInteger nitr;
	while((nitr = _keywords->Next(false,itr, key, val)) != -1) {
		itr = (SQInteger)nitr;
		if(((SQInteger)_integer(val)) == tok)
			return _stringval(key);
	}
	return nullptr;
}

void SQLexer::LexBlockComment()
{
	bool done = false;
	while(!done) {
		switch(CUR_CHAR) {
			case '*': { NEXT(); if(CUR_CHAR == '/') { done = true; NEXT(); }}; continue;
			case '\n': _currentline++; NEXT(); continue;
			case SQUIRREL_EOB: Error("missing \"*/\" in comment");
			default: NEXT();
		}
	}
}

SQInteger SQLexer::Lex()
{
	_lasttokenline = _currentline;
	while(CUR_CHAR != SQUIRREL_EOB) {
		switch(CUR_CHAR){
		case '\t': case '\r': case ' ': NEXT(); continue;
		case '\n':
			_currentline++;
			_prevtoken=_curtoken;
			_curtoken='\n';
			NEXT();
			_currentcolumn=1;
			continue;
		case '/':
			NEXT();
			switch(CUR_CHAR){
			case '*':
				NEXT();
				LexBlockComment();
				continue;
			case '/':
				do { NEXT(); } while (CUR_CHAR != '\n' && (!IS_EOB()));
				continue;
			case '=':
				NEXT();
				RETURN_TOKEN(TK_DIVEQ);
			case '>':
				NEXT();
				RETURN_TOKEN(TK_ATTR_CLOSE);
			default:
				RETURN_TOKEN('/');
			}
		case '=':
			NEXT();
			if (CUR_CHAR != '='){ RETURN_TOKEN('=') }
			else { NEXT(); RETURN_TOKEN(TK_EQ); }
		case '<':
			NEXT();
			if ( CUR_CHAR == '=' ) { NEXT(); RETURN_TOKEN(TK_LE) }
			else if ( CUR_CHAR == '-' ) { NEXT(); RETURN_TOKEN(TK_NEWSLOT); }
			else if ( CUR_CHAR == '<' ) { NEXT(); RETURN_TOKEN(TK_SHIFTL); }
			else if ( CUR_CHAR == '/' ) { NEXT(); RETURN_TOKEN(TK_ATTR_OPEN); }
			else { RETURN_TOKEN('<') }
		case '>':
			NEXT();
			if (CUR_CHAR == '='){ NEXT(); RETURN_TOKEN(TK_GE);}
			else if(CUR_CHAR == '>'){
				NEXT();
				if(CUR_CHAR == '>'){
					NEXT();
					RETURN_TOKEN(TK_USHIFTR);
				}
				RETURN_TOKEN(TK_SHIFTR);
			}
			else { RETURN_TOKEN('>') }
		case '!':
			NEXT();
			if (CUR_CHAR != '='){ RETURN_TOKEN('!')}
			else { NEXT(); RETURN_TOKEN(TK_NE); }
		case '@': {
			SQInteger stype;
			NEXT();
			if(CUR_CHAR != '"')
				Error("string expected");
			if((stype=ReadString('"',true))!=-1) {
				RETURN_TOKEN(stype);
			}
			Error("error parsing the string");
					   }
		case '"':
		case '\'': {
			SQInteger stype;
			if((stype=ReadString(CUR_CHAR,false))!=-1){
				RETURN_TOKEN(stype);
			}
			Error("error parsing the string");
			}
		case '{': case '}': case '(': case ')': case '[': case ']':
		case ';': case ',': case '?': case '^': case '~':
			{SQInteger ret = CUR_CHAR;
			NEXT(); RETURN_TOKEN(ret); }
		case '.':
			NEXT();
			if (CUR_CHAR != '.'){ RETURN_TOKEN('.') }
			NEXT();
			if (CUR_CHAR != '.'){ Error("invalid token '..'"); }
			NEXT();
			RETURN_TOKEN(TK_VARPARAMS);
		case '&':
			NEXT();
			if (CUR_CHAR != '&'){ RETURN_TOKEN('&') }
			else { NEXT(); RETURN_TOKEN(TK_AND); }
		case '|':
			NEXT();
			if (CUR_CHAR != '|'){ RETURN_TOKEN('|') }
			else { NEXT(); RETURN_TOKEN(TK_OR); }
		case ':':
			NEXT();
			if (CUR_CHAR != ':'){ RETURN_TOKEN(':') }
			else { NEXT(); RETURN_TOKEN(TK_DOUBLE_COLON); }
		case '*':
			NEXT();
			if (CUR_CHAR == '='){ NEXT(); RETURN_TOKEN(TK_MULEQ);}
			else RETURN_TOKEN('*');
		case '%':
			NEXT();
			if (CUR_CHAR == '='){ NEXT(); RETURN_TOKEN(TK_MODEQ);}
			else RETURN_TOKEN('%');
		case '-':
			NEXT();
			if (CUR_CHAR == '='){ NEXT(); RETURN_TOKEN(TK_MINUSEQ);}
			else if  (CUR_CHAR == '-'){ NEXT(); RETURN_TOKEN(TK_MINUSMINUS);}
			else RETURN_TOKEN('-');
		case '+':
			NEXT();
			if (CUR_CHAR == '='){ NEXT(); RETURN_TOKEN(TK_PLUSEQ);}
			else if (CUR_CHAR == '+'){ NEXT(); RETURN_TOKEN(TK_PLUSPLUS);}
			else RETURN_TOKEN('+');
		case SQUIRREL_EOB:
			return 0;
		default:{
				if (isdigit(CUR_CHAR)) {
					SQInteger ret = ReadNumber();
					RETURN_TOKEN(ret);
				}
				else if (isalpha(CUR_CHAR) || CUR_CHAR == '_') {
					SQInteger t = ReadID();
					RETURN_TOKEN(t);
				}
				else {
					SQInteger c = CUR_CHAR;
					if (iscntrl((int)c)) Error("unexpected character(control)");
					NEXT();
					RETURN_TOKEN(c);
				}
				RETURN_TOKEN(0);
			}
		}
	}
	return 0;
}

SQInteger SQLexer::GetIDType(SQChar *s)
{
	SQObjectPtr t;
	if(_keywords->Get(SQString::Create(_sharedstate, s), t)) {
		return SQInteger(_integer(t));
	}
	return TK_IDENTIFIER;
}


SQInteger SQLexer::ReadString(char32_t ndelim,bool verbatim)
{
	INIT_TEMP_STRING();
	NEXT();
	if(IS_EOB()) return -1;
	for(;;) {
		while(CUR_CHAR != ndelim) {
			switch(CUR_CHAR) {
			case SQUIRREL_EOB:
				Error("unfinished string");
				return -1;
			case '\n':
				if(!verbatim) Error("newline in a constant");
				APPEND_CHAR(CUR_CHAR); NEXT();
				_currentline++;
				break;
			case '\\':
				if(verbatim) {
					APPEND_CHAR('\\'); NEXT();
				}
				else {
					NEXT();
					switch(CUR_CHAR) {
					case 'x': NEXT(); {
						if(!isxdigit(CUR_CHAR)) Error("hexadecimal number expected");
						const SQInteger maxdigits = 4;
						SQChar temp[maxdigits+1];
						SQInteger n = 0;
						while(isxdigit(CUR_CHAR) && n < maxdigits) {
							temp[n] = CUR_CHAR;
							n++;
							NEXT();
						}
						temp[n] = 0;
						SQChar *sTemp;
						APPEND_CHAR((SQChar)strtoul(temp,&sTemp,16));
					}
				    break;
					case 't': APPEND_CHAR('\t'); NEXT(); break;
					case 'a': APPEND_CHAR('\a'); NEXT(); break;
					case 'b': APPEND_CHAR('\b'); NEXT(); break;
					case 'n': APPEND_CHAR('\n'); NEXT(); break;
					case 'r': APPEND_CHAR('\r'); NEXT(); break;
					case 'v': APPEND_CHAR('\v'); NEXT(); break;
					case 'f': APPEND_CHAR('\f'); NEXT(); break;
					case '0': APPEND_CHAR('\0'); NEXT(); break;
					case '\\': APPEND_CHAR('\\'); NEXT(); break;
					case '"': APPEND_CHAR('"'); NEXT(); break;
					case '\'': APPEND_CHAR('\''); NEXT(); break;
					default:
						Error("unrecognised escaper char");
					break;
					}
				}
				break;
			default:
				APPEND_CHAR(CUR_CHAR);
				NEXT();
			}
		}
		NEXT();
		if(verbatim && CUR_CHAR == '"') { //double quotation
			APPEND_CHAR(CUR_CHAR);
			NEXT();
		}
		else {
			break;
		}
	}
	TERMINATE_BUFFER();
	SQInteger len = _longstr.size()-1;
	if(ndelim == '\'') {
		if(len == 0) Error("empty constant");
		if(len > 1) Error("constant too long");
		_nvalue = _longstr[0];
		return TK_INTEGER;
	}
	_svalue = &_longstr[0];
	return TK_STRING_LITERAL;
}

void LexHexadecimal(const SQChar *s,SQUnsignedInteger *res)
{
	*res = 0;
	while(*s != 0)
	{
		if(isdigit(*s)) *res = (*res)*16+((*s++)-'0');
		else if(isxdigit(*s)) *res = (*res)*16+(toupper(*s++)-'A'+10);
		else { assert(0); }
	}
}

void LexInteger(const SQChar *s,SQUnsignedInteger *res)
{
	*res = 0;
	while(*s != 0)
	{
		*res = (*res)*10+((*s++)-'0');
	}
}

SQInteger scisodigit(SQChar c) { return c >= '0' && c <= '7'; }

void LexOctal(const SQChar *s,SQUnsignedInteger *res)
{
	*res = 0;
	while(*s != 0)
	{
		if(scisodigit(*s)) *res = (*res)*8+((*s++)-'0');
		else { assert(0); }
	}
}

SQInteger isexponent(SQInteger c) { return c == 'e' || c=='E'; }


#define MAX_HEX_DIGITS (sizeof(SQInteger)*2)
SQInteger SQLexer::ReadNumber()
{
#define TINT 1
#define TFLOAT 2
#define THEX 3
#define TSCIENTIFIC 4
#define TOCTAL 5
	SQInteger type = TINT, firstchar = CUR_CHAR;
	SQChar *sTemp;
	INIT_TEMP_STRING();
	NEXT();
	if(firstchar == '0' && (toupper(CUR_CHAR) == 'X' || scisodigit(CUR_CHAR)) ) {
		if(scisodigit(CUR_CHAR)) {
			type = TOCTAL;
			while(scisodigit(CUR_CHAR)) {
				APPEND_CHAR(CUR_CHAR);
				NEXT();
			}
			if(isdigit(CUR_CHAR)) Error("invalid octal number");
		}
		else {
			NEXT();
			type = THEX;
			while(isxdigit(CUR_CHAR)) {
				APPEND_CHAR(CUR_CHAR);
				NEXT();
			}
			if(_longstr.size() > MAX_HEX_DIGITS) Error("too many digits for an Hex number");
		}
	}
	else {
		APPEND_CHAR((int)firstchar);
		while (CUR_CHAR == '.' || isdigit(CUR_CHAR) || isexponent(CUR_CHAR)) {
            if(CUR_CHAR == '.' || isexponent(CUR_CHAR)) type = TFLOAT;
			if(isexponent(CUR_CHAR)) {
				if(type != TFLOAT) Error("invalid numeric format");
				type = TSCIENTIFIC;
				APPEND_CHAR(CUR_CHAR);
				NEXT();
				if(CUR_CHAR == '+' || CUR_CHAR == '-'){
					APPEND_CHAR(CUR_CHAR);
					NEXT();
				}
				if(!isdigit(CUR_CHAR)) Error("exponent expected");
			}

			APPEND_CHAR(CUR_CHAR);
			NEXT();
		}
	}
	TERMINATE_BUFFER();
	switch(type) {
	case TSCIENTIFIC:
	case TFLOAT:
		_fvalue = (SQFloat)strtod(&_longstr[0],&sTemp);
		return TK_FLOAT;
	case TINT:
		LexInteger(&_longstr[0],(SQUnsignedInteger *)&_nvalue);
		return TK_INTEGER;
	case THEX:
		LexHexadecimal(&_longstr[0],(SQUnsignedInteger *)&_nvalue);
		return TK_INTEGER;
	case TOCTAL:
		LexOctal(&_longstr[0],(SQUnsignedInteger *)&_nvalue);
		return TK_INTEGER;
	}
	return 0;
}

SQInteger SQLexer::ReadID()
{
	SQInteger res;
	INIT_TEMP_STRING();
	do {
		APPEND_CHAR(CUR_CHAR);
		NEXT();
	} while(isalnum(CUR_CHAR) || CUR_CHAR == '_');
	TERMINATE_BUFFER();
	res = GetIDType(&_longstr[0]);
	if(res == TK_IDENTIFIER || res == TK_CONSTRUCTOR) {
		_svalue = &_longstr[0];
	}
	return res;
}
