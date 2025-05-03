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

#include "../../../core/utf8.hpp"
#include "../../../core/string_consumer.hpp"

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
	auto [buf, chars] = EncodeUtf8(c);
	for (size_t i = 0; i < chars; i++) {
		_longstr.push_back(buf[i]);
	}
}

SQLexer::SQLexer(SQSharedState *ss, SQLEXREADFUNC rg, SQUserPointer up)
{
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

	_nvalue = 0;
	_fvalue = 0;

	Next();
}

void SQLexer::Next()
{
	char32_t t = _readf(_up);
	if(t > MAX_CHAR) throw CompileException("Invalid character");
	if(t != 0) {
		_currdata = t;
		return;
	}
	_currdata = SQUIRREL_EOB;
}

std::optional<std::string_view> SQLexer::Tok2Str(SQInteger tok)
{
	SQObjectPtr itr, key, val;
	SQInteger nitr;
	while((nitr = _keywords->Next(false,itr, key, val)) != -1) {
		itr = (SQInteger)nitr;
		if(((SQInteger)_integer(val)) == tok)
			return _stringval(key);
	}
	return std::nullopt;
}

void SQLexer::LexBlockComment()
{
	bool done = false;
	while(!done) {
		switch(CUR_CHAR) {
			case '*': { NEXT(); if(CUR_CHAR == '/') { done = true; NEXT(); }}; continue;
			case '\n': _currentline++; NEXT(); continue;
			case SQUIRREL_EOB: throw CompileException("missing \"*/\" in comment");
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
				throw CompileException("string expected");
			if((stype=ReadString('"',true))!=-1) {
				RETURN_TOKEN(stype);
			}
			throw CompileException("error parsing the string");
					   }
		case '"':
		case '\'': {
			SQInteger stype;
			if((stype=ReadString(CUR_CHAR,false))!=-1){
				RETURN_TOKEN(stype);
			}
			throw CompileException("error parsing the string");
			}
		case '{': case '}': case '(': case ')': case '[': case ']':
		case ';': case ',': case '?': case '^': case '~':
			{SQInteger ret = CUR_CHAR;
			NEXT(); RETURN_TOKEN(ret); }
		case '.':
			NEXT();
			if (CUR_CHAR != '.'){ RETURN_TOKEN('.') }
			NEXT();
			if (CUR_CHAR != '.'){ throw CompileException("invalid token '..'"); }
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
					if (iscntrl((int)c)) throw CompileException("unexpected character(control)");
					NEXT();
					RETURN_TOKEN(c);
				}
				RETURN_TOKEN(0);
			}
		}
	}
	return 0;
}

SQInteger SQLexer::GetIDType(std::string_view s)
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
				throw CompileException("unfinished string");
				return -1;
			case '\n':
				if(!verbatim) throw CompileException("newline in a constant");
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
						if(!isxdigit(CUR_CHAR)) throw CompileException("hexadecimal number expected");
						const SQInteger maxdigits = 4;
						char temp[maxdigits];
						size_t n = 0;
						while(isxdigit(CUR_CHAR) && n < maxdigits) {
							temp[n] = CUR_CHAR;
							n++;
							NEXT();
						}
						auto val = ParseInteger(std::string_view{temp, n}, 16);
						if (!val.has_value()) throw CompileException("hexadecimal number expected");
						APPEND_CHAR(static_cast<char>(*val));
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
						throw CompileException("unrecognised escaper char");
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
	if(ndelim == '\'') {
		if(_longstr.empty()) throw CompileException("empty constant");
		if(_longstr.size() > 1) throw CompileException("constant too long");
		_nvalue = _longstr[0];
		return TK_INTEGER;
	}
	return TK_STRING_LITERAL;
}

SQInteger scisodigit(char c) { return c >= '0' && c <= '7'; }

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
	INIT_TEMP_STRING();
	NEXT();
	if(firstchar == '0' && (toupper(CUR_CHAR) == 'X' || scisodigit(CUR_CHAR)) ) {
		if(scisodigit(CUR_CHAR)) {
			type = TOCTAL;
			while(scisodigit(CUR_CHAR)) {
				APPEND_CHAR(CUR_CHAR);
				NEXT();
			}
			if(isdigit(CUR_CHAR)) throw CompileException("invalid octal number");
		}
		else {
			NEXT();
			type = THEX;
			while(isxdigit(CUR_CHAR)) {
				APPEND_CHAR(CUR_CHAR);
				NEXT();
			}
			if(_longstr.size() > MAX_HEX_DIGITS) throw CompileException("too many digits for an Hex number");
		}
	}
	else {
		APPEND_CHAR((int)firstchar);
		while (CUR_CHAR == '.' || isdigit(CUR_CHAR) || isexponent(CUR_CHAR)) {
            if(CUR_CHAR == '.' || isexponent(CUR_CHAR)) type = TFLOAT;
			if(isexponent(CUR_CHAR)) {
				if(type != TFLOAT) throw CompileException("invalid numeric format");
				type = TSCIENTIFIC;
				APPEND_CHAR(CUR_CHAR);
				NEXT();
				if(CUR_CHAR == '+' || CUR_CHAR == '-'){
					APPEND_CHAR(CUR_CHAR);
					NEXT();
				}
				if(!isdigit(CUR_CHAR)) throw CompileException("exponent expected");
			}

			APPEND_CHAR(CUR_CHAR);
			NEXT();
		}
	}
	switch(type) {
	case TSCIENTIFIC:
	case TFLOAT: {
		std::string str{View()};
		char *sTemp;
		_fvalue = (SQFloat)strtod(str.c_str(),&sTemp);
		return TK_FLOAT;
	}
	case TINT:
		_nvalue = ParseInteger<SQUnsignedInteger>(View(), 10).value_or(0);
		return TK_INTEGER;
	case THEX:
		_nvalue = ParseInteger<SQUnsignedInteger>(View(), 16).value_or(0);
		return TK_INTEGER;
	case TOCTAL:
		_nvalue = ParseInteger<SQUnsignedInteger>(View(), 8).value_or(0);
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
	res = GetIDType(View());
	return res;
}
