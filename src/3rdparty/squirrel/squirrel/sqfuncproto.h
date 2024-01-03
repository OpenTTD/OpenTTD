/*	see copyright notice in squirrel.h */
#ifndef _SQFUNCTION_H_
#define _SQFUNCTION_H_

#include "sqopcodes.h"

enum SQOuterType {
	otLOCAL = 0,
	otSYMBOL = 1,
	otOUTER = 2,
};

struct SQOuterVar
{

	SQOuterVar() : _type(otLOCAL) {}
	SQOuterVar(const SQObjectPtr &name,const SQObjectPtr &src,SQOuterType t)
	{
		_name = name;
		_src=src;
		_type=t;
	}
	SQOuterType _type;
	SQObjectPtr _name;
	SQObjectPtr _src;
};

struct SQLocalVarInfo
{
	SQLocalVarInfo():_start_op(0),_end_op(0), _pos(0){}
	SQObjectPtr _name;
	SQUnsignedInteger _start_op;
	SQUnsignedInteger _end_op;
	SQUnsignedInteger _pos;
};

struct SQLineInfo { SQInteger _line;SQInteger _op; };

typedef sqvector<SQOuterVar> SQOuterVarVec;
typedef sqvector<SQLocalVarInfo> SQLocalVarInfoVec;
typedef sqvector<SQLineInfo> SQLineInfoVec;

#define _FUNC_SIZE(ni,nl,nparams,nfuncs,nouters,nlineinf,localinf,defparams) (sizeof(SQFunctionProto) \
		+((ni-1)*sizeof(SQInstruction))+(nl*sizeof(SQObjectPtr)) \
		+(nparams*sizeof(SQObjectPtr))+(nfuncs*sizeof(SQObjectPtr)) \
		+(nouters*sizeof(SQOuterVar))+(nlineinf*sizeof(SQLineInfo)) \
		+(localinf*sizeof(SQLocalVarInfo))+(defparams*sizeof(SQInteger)))

#define _CONSTRUCT_VECTOR(type,size,ptr) { \
	for(SQInteger n = 0; n < size; n++) { \
			new (&ptr[n]) type(); \
		} \
}

#define _DESTRUCT_VECTOR(type,size,ptr) { \
	for(SQInteger nl = 0; nl < size; nl++) { \
			ptr[nl].~type(); \
	} \
}
struct SQFunctionProto : public SQRefCounted
{
private:
	SQFunctionProto(SQInteger ninstructions,
		SQInteger nliterals,SQInteger nparameters,
		SQInteger nfunctions,SQInteger noutervalues,
		SQInteger nlineinfos,SQInteger nlocalvarinfos,SQInteger ndefaultparams)
	{
		_stacksize=0;
		_bgenerator=false;
		_varparams = false;
		_ninstructions = ninstructions;
		_literals = (SQObjectPtr*)&_instructions[ninstructions];
		_nliterals = nliterals;
		_parameters = (SQObjectPtr*)&_literals[nliterals];
		_nparameters = nparameters;
		_functions = (SQObjectPtr*)&_parameters[nparameters];
		_nfunctions = nfunctions;
		_outervalues = (SQOuterVar*)&_functions[nfunctions];
		_noutervalues = noutervalues;
		_lineinfos = (SQLineInfo *)&_outervalues[noutervalues];
		_nlineinfos = nlineinfos;
		_localvarinfos = (SQLocalVarInfo *)&_lineinfos[nlineinfos];
		_nlocalvarinfos = nlocalvarinfos;
		_defaultparams = (SQInteger *)&_localvarinfos[nlocalvarinfos];
		_ndefaultparams = ndefaultparams;

		_CONSTRUCT_VECTOR(SQObjectPtr,_nliterals,_literals);
		_CONSTRUCT_VECTOR(SQObjectPtr,_nparameters,_parameters);
		_CONSTRUCT_VECTOR(SQObjectPtr,_nfunctions,_functions);
		_CONSTRUCT_VECTOR(SQOuterVar,_noutervalues,_outervalues);
		//_CONSTRUCT_VECTOR(SQLineInfo,_nlineinfos,_lineinfos); //not required are 2 integers
		_CONSTRUCT_VECTOR(SQLocalVarInfo,_nlocalvarinfos,_localvarinfos);
	}
public:
	static SQFunctionProto *Create(SQInteger ninstructions,
		SQInteger nliterals,SQInteger nparameters,
		SQInteger nfunctions,SQInteger noutervalues,
		SQInteger nlineinfos,SQInteger nlocalvarinfos,SQInteger ndefaultparams)
	{
		SQFunctionProto *f;
		//I compact the whole class and members in a single memory allocation
		f = (SQFunctionProto *)sq_vm_malloc(_FUNC_SIZE(ninstructions,nliterals,nparameters,nfunctions,noutervalues,nlineinfos,nlocalvarinfos,ndefaultparams));
		new (f) SQFunctionProto(ninstructions, nliterals, nparameters, nfunctions, noutervalues, nlineinfos, nlocalvarinfos, ndefaultparams);
		return f;
	}
	void Release() override {
		_DESTRUCT_VECTOR(SQObjectPtr,_nliterals,_literals);
		_DESTRUCT_VECTOR(SQObjectPtr,_nparameters,_parameters);
		_DESTRUCT_VECTOR(SQObjectPtr,_nfunctions,_functions);
		_DESTRUCT_VECTOR(SQOuterVar,_noutervalues,_outervalues);
		//_DESTRUCT_VECTOR(SQLineInfo,_nlineinfos,_lineinfos); //not required are 2 integers
		_DESTRUCT_VECTOR(SQLocalVarInfo,_nlocalvarinfos,_localvarinfos);
		SQInteger size = _FUNC_SIZE(_ninstructions,_nliterals,_nparameters,_nfunctions,_noutervalues,_nlineinfos,_nlocalvarinfos,_ndefaultparams);
		this->~SQFunctionProto();
		sq_vm_free(this,size);
	}
	const SQChar* GetLocal(SQVM *v,SQUnsignedInteger stackbase,SQUnsignedInteger nseq,SQUnsignedInteger nop);
	SQInteger GetLine(SQInstruction *curr);
	bool Save(SQVM *v,SQUserPointer up,SQWRITEFUNC write);
	static bool Load(SQVM *v,SQUserPointer up,SQREADFUNC read,SQObjectPtr &ret);

	SQObjectPtr _sourcename;
	SQObjectPtr _name;
    SQInteger _stacksize;
	bool _bgenerator;
	bool _varparams;

	SQInteger _nlocalvarinfos;
	SQLocalVarInfo *_localvarinfos;

	SQInteger _nlineinfos;
	SQLineInfo *_lineinfos;

	SQInteger _nliterals;
	SQObjectPtr *_literals;

	SQInteger _nparameters;
	SQObjectPtr *_parameters;

	SQInteger _nfunctions;
	SQObjectPtr *_functions;

	SQInteger _noutervalues;
	SQOuterVar *_outervalues;

	SQInteger _ndefaultparams;
	SQInteger *_defaultparams;

	SQInteger _ninstructions;
	SQInstruction _instructions[1];
};

#endif //_SQFUNCTION_H_
