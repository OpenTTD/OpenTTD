/*	see copyright notice in squirrel.h */
#ifndef _SQLEXER_H_
#define _SQLEXER_H_

struct SQLexer
{
	~SQLexer();
	SQLexer(SQSharedState *ss,SQLEXREADFUNC rg,SQUserPointer up);
	SQInteger Lex();
	std::optional<std::string_view> Tok2Str(SQInteger tok);
private:
	SQInteger GetIDType(std::string_view s);
	SQInteger ReadString(char32_t ndelim,bool verbatim);
	SQInteger ReadNumber();
	void LexBlockComment();
	SQInteger ReadID();
	void Next();
	SQInteger _curtoken;
	SQTable *_keywords;
	void INIT_TEMP_STRING() { _longstr.resize(0); }
	void APPEND_CHAR(char32_t c);

public:
	SQInteger _prevtoken;
	SQInteger _currentline;
	SQInteger _lasttokenline;
	SQInteger _currentcolumn;
	SQInteger _nvalue;
	SQFloat _fvalue;
	SQLEXREADFUNC _readf;
	SQUserPointer _up;
	char32_t _currdata;
	SQSharedState *_sharedstate;
	sqvector<char> _longstr;

	std::string_view View() const { return std::string_view(_longstr._vals, _longstr.size()); }
};

#endif
