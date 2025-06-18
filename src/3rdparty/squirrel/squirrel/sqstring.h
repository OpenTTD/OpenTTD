/*	see copyright notice in squirrel.h */
#ifndef _SQSTRING_H_
#define _SQSTRING_H_

struct SQString : public SQRefCounted
{
	SQString(std::string_view str);
	~SQString(){}
public:
	static SQString *Create(SQSharedState *ss, std::string_view str);
	SQInteger Next(const SQObjectPtr &refpos, SQObjectPtr &outkey, SQObjectPtr &outval);
	void Release() override;
	SQSharedState *_sharedstate;
	SQString *_next; //chain for the string table
	std::size_t _hash;

	std::string_view View() const { return std::string_view(this->_val, this->_len); }
	std::span<char> Span() { return std::span<char>(this->_val, this->_len); }
private:
	SQInteger _len;
	char _val[1];
};



#endif //_SQSTRING_H_
