/*	see copyright notice in squirrel.h */
#ifndef _SQOPCODES_H_
#define _SQOPCODES_H_

#define MAX_FUNC_STACKSIZE 0xFF
#define MAX_LITERALS ((SQInteger)0x7FFFFFFF)

enum BitWiseOP {
	BW_AND = 0,
	BW_OR = 2,
	BW_XOR = 3,
	BW_SHIFTL = 4,
	BW_SHIFTR = 5,
	BW_USHIFTR = 6
};

enum CmpOP {
	CMP_G = 0,
	CMP_GE = 2,
	CMP_L = 3,
	CMP_LE = 4
};
enum SQOpcode
{
	_OP_LINE=				0x00,
	_OP_LOAD=				0x01,
	_OP_LOADINT=			0x02,
	_OP_LOADFLOAT=			0x03,
	_OP_DLOAD=				0x04,
	_OP_TAILCALL=			0x05,
	_OP_CALL=				0x06,
	_OP_PREPCALL=			0x07,
	_OP_PREPCALLK=			0x08,
	_OP_GETK=				0x09,
	_OP_MOVE=				0x0A,
	_OP_NEWSLOT=			0x0B,
	_OP_DELETE=				0x0C,
	_OP_SET=				0x0D,
	_OP_GET=				0x0E,
	_OP_EQ=					0x0F,
	_OP_NE=					0x10,
	_OP_ARITH=				0x11,
	_OP_BITW=				0x12,
	_OP_RETURN=				0x13,
	_OP_LOADNULLS=			0x14,
	_OP_LOADROOTTABLE=		0x15,
	_OP_LOADBOOL=			0x16,
	_OP_DMOVE=				0x17,
	_OP_JMP=				0x18,
	_OP_JNZ=				0x19,
	_OP_JZ=					0x1A,
	_OP_LOADFREEVAR=		0x1B,
	_OP_VARGC=				0x1C,
	_OP_GETVARGV=			0x1D,
	_OP_NEWTABLE=			0x1E,
	_OP_NEWARRAY=			0x1F,
	_OP_APPENDARRAY=		0x20,
	_OP_GETPARENT=			0x21,
	_OP_COMPARITH=			0x22,
	_OP_COMPARITHL=			0x23,
	_OP_INC=				0x24,
	_OP_INCL=				0x25,
	_OP_PINC=				0x26,
	_OP_PINCL=				0x27,
	_OP_CMP=				0x28,
	_OP_EXISTS=				0x29,
	_OP_INSTANCEOF=			0x2A,
	_OP_AND=				0x2B,
	_OP_OR=					0x2C,
	_OP_NEG=				0x2D,
	_OP_NOT=				0x2E,
	_OP_BWNOT=				0x2F,
	_OP_CLOSURE=			0x30,
	_OP_YIELD=				0x31,
	_OP_RESUME=				0x32,
	_OP_FOREACH=			0x33,
	_OP_POSTFOREACH=		0x34,
	_OP_DELEGATE=			0x35,
	_OP_CLONE=				0x36,
	_OP_TYPEOF=				0x37,
	_OP_PUSHTRAP=			0x38,
	_OP_POPTRAP=			0x39,
	_OP_THROW=				0x3A,
	_OP_CLASS=				0x3B,
	_OP_NEWSLOTA=			0x3C,
	_OP_SCOPE_END=		0x3D,
};

struct SQInstructionDesc {
	const SQChar *name;
};

struct SQInstruction
{
	SQInstruction(){};
	SQInstruction(SQOpcode _op,SQInteger a0=0,SQInteger a1=0,SQInteger a2=0,SQInteger a3=0)
	{	op = _op;
		_arg0 = (unsigned char)a0;_arg1 = (SQInt32)a1;
		_arg2 = (unsigned char)a2;_arg3 = (unsigned char)a3;
	}


	SQInt32 _arg1;
	unsigned char op;
	unsigned char _arg0;
	unsigned char _arg2;
	unsigned char _arg3;
};

#include "squtils.h"
typedef sqvector<SQInstruction> SQInstructionVec;

#define NEW_SLOT_ATTRIBUTES_FLAG	0x01
#define NEW_SLOT_STATIC_FLAG		0x02

#endif // _SQOPCODES_H_
