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
	SQInteger _len;
	std::size_t _hash;
	SQChar _val[1];
};



#endif //_SQSTRING_H_
