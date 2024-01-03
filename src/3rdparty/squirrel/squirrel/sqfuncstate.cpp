/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"
#include "../../fmt/format.h"

#include "sqpcheader.h"
#include "sqcompiler.h"
#include "sqfuncproto.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqopcodes.h"
#include "sqfuncstate.h"

#include "../../../safeguards.h"

#ifdef _DEBUG_DUMP
SQInstructionDesc g_InstrDesc[]={
	{"_OP_LINE"},
	{"_OP_LOAD"},
	{"_OP_LOADINT"},
	{"_OP_LOADFLOAT"},
	{"_OP_DLOAD"},
	{"_OP_TAILCALL"},
	{"_OP_CALL"},
	{"_OP_PREPCALL"},
	{"_OP_PREPCALLK"},
	{"_OP_GETK"},
	{"_OP_MOVE"},
	{"_OP_NEWSLOT"},
	{"_OP_DELETE"},
	{"_OP_SET"},
	{"_OP_GET"},
	{"_OP_EQ"},
	{"_OP_NE"},
	{"_OP_ARITH"},
	{"_OP_BITW"},
	{"_OP_RETURN"},
	{"_OP_LOADNULLS"},
	{"_OP_LOADROOTTABLE"},
	{"_OP_LOADBOOL"},
	{"_OP_DMOVE"},
	{"_OP_JMP"},
	{"_OP_JNZ"},
	{"_OP_JZ"},
	{"_OP_LOADFREEVAR"},
	{"_OP_VARGC"},
	{"_OP_GETVARGV"},
	{"_OP_NEWTABLE"},
	{"_OP_NEWARRAY"},
	{"_OP_APPENDARRAY"},
	{"_OP_GETPARENT"},
	{"_OP_COMPARITH"},
	{"_OP_COMPARITHL"},
	{"_OP_INC"},
	{"_OP_INCL"},
	{"_OP_PINC"},
	{"_OP_PINCL"},
	{"_OP_CMP"},
	{"_OP_EXISTS"},
	{"_OP_INSTANCEOF"},
	{"_OP_AND"},
	{"_OP_OR"},
	{"_OP_NEG"},
	{"_OP_NOT"},
	{"_OP_BWNOT"},
	{"_OP_CLOSURE"},
	{"_OP_YIELD"},
	{"_OP_RESUME"},
	{"_OP_FOREACH"},
	{"_OP_POSTFOREACH"},
	{"_OP_DELEGATE"},
	{"_OP_CLONE"},
	{"_OP_TYPEOF"},
	{"_OP_PUSHTRAP"},
	{"_OP_POPTRAP"},
	{"_OP_THROW"},
	{"_OP_CLASS"},
	{"_OP_NEWSLOTA"},
	{"_OP_SCOPE_END"}
};

void DumpLiteral(SQObjectPtr &o)
{
	switch(type(o)){
		case OT_STRING:	fmt::print("\"{}\"",_stringval(o));break;
		case OT_FLOAT: fmt::print("{{{}}}",_float(o));break;
		case OT_INTEGER: fmt::print("{{{}}}",_integer(o));break;
		case OT_BOOL: fmt::print(_integer(o)?"true":"false");break;
		default: fmt::print("({} {})",GetTypeName(o),(size_t)(void*)_rawval(o));break; break; //shut up compiler
	}
}
#endif

SQFuncState::SQFuncState(SQSharedState *ss,SQFuncState *parent,CompilerErrorFunc efunc,void *ed)
{
		_nliterals = 0;
		_literals = SQTable::Create(ss,0);
		_strings =  SQTable::Create(ss,0);
		_sharedstate = ss;
		_lastline = 0;
		_optimization = true;
		_parent = parent;
		_stacksize = 0;
		_traps = 0;
		_returnexp = 0;
		_varparams = false;
		_errfunc = efunc;
		_errtarget = ed;
		_bgenerator = false;

}

void SQFuncState::Error(const SQChar *err)
{
	_errfunc(_errtarget,err);
}

#ifdef _DEBUG_DUMP
void SQFuncState::Dump(SQFunctionProto *func)
{
	SQUnsignedInteger n=0,i;
	SQInteger si;
	printf("SQInstruction sizeof %d\n",sizeof(SQInstruction));
	printf("SQObject sizeof %d\n",sizeof(SQObject));
	printf("--------------------------------------------------------------------\n");
	printf("*****FUNCTION [%s]\n",type(func->_name)==OT_STRING?_stringval(func->_name):"unknown");
	printf("-----LITERALS\n");
	SQObjectPtr refidx,key,val;
	SQInteger idx;
	SQObjectPtrVec templiterals;
	templiterals.resize(_nliterals);
	while((idx=_table(_literals)->Next(false,refidx,key,val))!=-1) {
		refidx=idx;
		templiterals[_integer(val)]=key;
	}
	for(i=0;i<templiterals.size();i++){
		printf("[%d] ",n);
		DumpLiteral(templiterals[i]);
		printf("\n");
		n++;
	}
	printf("-----PARAMS\n");
	if(_varparams)
		printf("<<VARPARAMS>>\n");
	n=0;
	for(i=0;i<_parameters.size();i++){
		printf("[%d] ",n);
		DumpLiteral(_parameters[i]);
		printf("\n");
		n++;
	}
	printf("-----LOCALS\n");
	for(si=0;si<func->_nlocalvarinfos;si++){
		SQLocalVarInfo lvi=func->_localvarinfos[si];
		printf("[%d] %s \t%d %d\n",lvi._pos,_stringval(lvi._name),lvi._start_op,lvi._end_op);
		n++;
	}
	printf("-----LINE INFO\n");
	for(i=0;i<_lineinfos.size();i++){
		SQLineInfo li=_lineinfos[i];
		printf("op [%d] line [%d] \n",li._op,li._line);
		n++;
	}
	printf("-----dump\n");
	n=0;
	for(i=0;i<_instructions.size();i++){
		SQInstruction &inst=_instructions[i];
		if(inst.op==_OP_LOAD || inst.op==_OP_DLOAD || inst.op==_OP_PREPCALLK || inst.op==_OP_GETK ){

			SQInteger lidx = inst._arg1;
			printf("[%03d] %15s %d ",n,g_InstrDesc[inst.op].name,inst._arg0);
			if(lidx >= 0xFFFFFFFF)
				printf("null");
			else {
				SQInteger refidx;
				SQObjectPtr val,key,refo;
				while(((refidx=_table(_literals)->Next(false,refo,key,val))!= -1) && (_integer(val) != lidx)) {
					refo = refidx;
				}
				DumpLiteral(key);
			}
			if(inst.op != _OP_DLOAD) {
				printf(" %d %d \n",inst._arg2,inst._arg3);
			}
			else {
				printf(" %d ",inst._arg2);
				lidx = inst._arg3;
				if(lidx >= 0xFFFFFFFF)
					printf("null");
				else {
					SQInteger refidx;
					SQObjectPtr val,key,refo;
					while(((refidx=_table(_literals)->Next(false,refo,key,val))!= -1) && (_integer(val) != lidx)) {
						refo = refidx;
				}
				DumpLiteral(key);
				printf("\n");
			}
			}
		}
		else if(inst.op==_OP_LOADFLOAT) {
			printf("[%03d] %15s %d %f %d %d\n",n,g_InstrDesc[inst.op].name,inst._arg0,*((SQFloat*)&inst._arg1),inst._arg2,inst._arg3);
		}
		else if(inst.op==_OP_ARITH){
			printf("[%03d] %15s %d %d %d %c\n",n,g_InstrDesc[inst.op].name,inst._arg0,inst._arg1,inst._arg2,inst._arg3);
		}
		else
			printf("[%03d] %15s %d %d %d %d\n",n,g_InstrDesc[inst.op].name,inst._arg0,inst._arg1,inst._arg2,inst._arg3);
		n++;
	}
	printf("-----\n");
	printf("stack size[%d]\n",func->_stacksize);
	printf("--------------------------------------------------------------------\n\n");
}
#endif

SQInteger SQFuncState::GetNumericConstant(const SQInteger cons)
{
	return GetConstant(SQObjectPtr(cons));
}

SQInteger SQFuncState::GetNumericConstant(const SQFloat cons)
{
	return GetConstant(SQObjectPtr(cons));
}

SQInteger SQFuncState::GetConstant(const SQObject &cons)
{
	SQObjectPtr val;
	if(!_table(_literals)->Get(cons,val))
	{
		val = _nliterals;
		_table(_literals)->NewSlot(cons,val);
		_nliterals++;
		if(_nliterals > MAX_LITERALS) {
			val.Null();
			Error("internal compiler error: too many literals");
		}
	}
	return _integer(val);
}

void SQFuncState::SetIntructionParams(SQInteger pos,SQInteger arg0,SQInteger arg1,SQInteger arg2,SQInteger arg3)
{
	_instructions[pos]._arg0=(unsigned char)*((SQUnsignedInteger *)&arg0);
	_instructions[pos]._arg1=(SQInt32)*((SQUnsignedInteger *)&arg1);
	_instructions[pos]._arg2=(unsigned char)*((SQUnsignedInteger *)&arg2);
	_instructions[pos]._arg3=(unsigned char)*((SQUnsignedInteger *)&arg3);
}

void SQFuncState::SetIntructionParam(SQInteger pos,SQInteger arg,SQInteger val)
{
	switch(arg){
		case 0:_instructions[pos]._arg0=(unsigned char)*((SQUnsignedInteger *)&val);break;
		case 1:case 4:_instructions[pos]._arg1=(SQInt32)*((SQUnsignedInteger *)&val);break;
		case 2:_instructions[pos]._arg2=(unsigned char)*((SQUnsignedInteger *)&val);break;
		case 3:_instructions[pos]._arg3=(unsigned char)*((SQUnsignedInteger *)&val);break;
	};
}

SQInteger SQFuncState::AllocStackPos()
{
	SQInteger npos=_vlocals.size();
	_vlocals.push_back(SQLocalVarInfo());
	if(_vlocals.size()>((SQUnsignedInteger)_stacksize)) {
		if(_stacksize>MAX_FUNC_STACKSIZE) Error("internal compiler error: too many locals");
		_stacksize=_vlocals.size();
	}
	return npos;
}

SQInteger SQFuncState::PushTarget(SQInteger n)
{
	if(n!=-1){
		_targetstack.push_back(n);
		return n;
	}
	n=AllocStackPos();
	_targetstack.push_back(n);
	return n;
}

SQInteger SQFuncState::GetUpTarget(SQInteger n){
	return _targetstack[((_targetstack.size()-1)-n)];
}

SQInteger SQFuncState::TopTarget(){
	return _targetstack.back();
}
SQInteger SQFuncState::PopTarget()
{
	SQInteger npos=_targetstack.back();
	SQLocalVarInfo t=_vlocals[_targetstack.back()];
	if(type(t._name)==OT_NULL){
		_vlocals.pop_back();
	}
	_targetstack.pop_back();
	return npos;
}

SQInteger SQFuncState::GetStackSize()
{
	return _vlocals.size();
}

void SQFuncState::SetStackSize(SQInteger n)
{
	SQInteger size=_vlocals.size();
	while(size>n){
		size--;
		SQLocalVarInfo lvi=_vlocals.back();
		if(type(lvi._name)!=OT_NULL){
			lvi._end_op=GetCurrentPos();
			_localvarinfos.push_back(lvi);
		}
		_vlocals.pop_back();
	}
}

bool SQFuncState::IsConstant(const SQObject &name,SQObject &e)
{
	SQObjectPtr val;
	if(_table(_sharedstate->_consts)->Get(name,val)) {
		e = val;
		return true;
	}
	return false;
}

bool SQFuncState::IsLocal(SQUnsignedInteger stkpos)
{
	if(stkpos>=_vlocals.size())return false;
	else if(type(_vlocals[stkpos]._name)!=OT_NULL)return true;
	return false;
}

SQInteger SQFuncState::PushLocalVariable(const SQObject &name)
{
	SQInteger pos=_vlocals.size();
	SQLocalVarInfo lvi;
	lvi._name=name;
	lvi._start_op=GetCurrentPos()+1;
	lvi._pos=_vlocals.size();
	_vlocals.push_back(lvi);
	if(_vlocals.size()>((SQUnsignedInteger)_stacksize))_stacksize=_vlocals.size();

	return pos;
}

SQInteger SQFuncState::GetLocalVariable(const SQObject &name)
{
	SQInteger locals=_vlocals.size();
	while(locals>=1){
		if(type(_vlocals[locals-1]._name)==OT_STRING && _string(_vlocals[locals-1]._name)==_string(name)){
			return locals-1;
		}
		locals--;
	}
	return -1;
}

SQInteger SQFuncState::GetOuterVariable(const SQObject &name)
{
	SQInteger outers = _outervalues.size();
	for(SQInteger i = 0; i<outers; i++) {
		if(_string(_outervalues[i]._name) == _string(name))
			return i;
	}
	return -1;
}

void SQFuncState::AddOuterValue(const SQObject &name)
{
	SQInteger pos=-1;
	if(_parent) {
		pos = _parent->GetLocalVariable(name);
		if(pos == -1) {
			pos = _parent->GetOuterVariable(name);
			if(pos != -1) {
				_outervalues.push_back(SQOuterVar(name,SQObjectPtr(SQInteger(pos)),otOUTER)); //local
				return;
			}
		}
		else {
			_outervalues.push_back(SQOuterVar(name,SQObjectPtr(SQInteger(pos)),otLOCAL)); //local
			return;
		}
	}
	_outervalues.push_back(SQOuterVar(name,name,otSYMBOL)); //global
}

void SQFuncState::AddParameter(const SQObject &name)
{
	PushLocalVariable(name);
	_parameters.push_back(name);
}

void SQFuncState::AddLineInfos(SQInteger line,bool lineop,bool force)
{
	if(_lastline!=line || force){
		SQLineInfo li;
		li._line=line;li._op=(GetCurrentPos()+1);
		if(lineop)AddInstruction(_OP_LINE,0,line);
		_lineinfos.push_back(li);
		_lastline=line;
	}
}

void SQFuncState::AddInstruction(SQInstruction &i)
{
	SQInteger size = _instructions.size();
	if(size > 0 && _optimization){ //simple optimizer
		SQInstruction &pi = _instructions[size-1];//previous instruction
		switch(i.op) {
		case _OP_RETURN:
			if( _parent && i._arg0 != MAX_FUNC_STACKSIZE && pi.op == _OP_CALL && _returnexp < size-1) {
				pi.op = _OP_TAILCALL;
			}
		break;
		case _OP_GET:
			if( pi.op == _OP_LOAD && pi._arg0 == i._arg2 && (!IsLocal(pi._arg0))){
				pi._arg2 = (unsigned char)i._arg1;
				pi.op = _OP_GETK;
				pi._arg0 = i._arg0;

				return;
			}
		break;
		case _OP_PREPCALL:
			if( pi.op == _OP_LOAD  && pi._arg0 == i._arg1 && (!IsLocal(pi._arg0))){
				pi.op = _OP_PREPCALLK;
				pi._arg0 = i._arg0;
				pi._arg2 = i._arg2;
				pi._arg3 = i._arg3;
				return;
			}
			break;
		case _OP_APPENDARRAY:
			if(pi.op == _OP_LOAD && pi._arg0 == i._arg1 && (!IsLocal(pi._arg0))){
				pi.op = _OP_APPENDARRAY;
				pi._arg0 = i._arg0;
				pi._arg2 = MAX_FUNC_STACKSIZE;
				pi._arg3 = MAX_FUNC_STACKSIZE;
				return;
			}
			break;
		case _OP_MOVE:
			if((pi.op == _OP_GET || pi.op == _OP_ARITH || pi.op == _OP_BITW) && (pi._arg0 == i._arg1))
			{
				pi._arg0 = i._arg0;
				_optimization = false;
				return;
			}

			if(pi.op == _OP_MOVE)
			{
				pi.op = _OP_DMOVE;
				pi._arg2 = i._arg0;
				pi._arg3 = (unsigned char)i._arg1;
				return;
			}
			break;
		case _OP_LOAD:
			if(pi.op == _OP_LOAD && i._arg1 < 256) {
				pi.op = _OP_DLOAD;
				pi._arg2 = i._arg0;
				pi._arg3 = (unsigned char)i._arg1;
				return;
			}
			break;
		case _OP_EQ:case _OP_NE:
			if(pi.op == _OP_LOAD && pi._arg0 == i._arg1 && (!IsLocal(pi._arg0) ))
			{
				pi.op = i.op;
				pi._arg0 = i._arg0;
				pi._arg2 = i._arg2;
				pi._arg3 = MAX_FUNC_STACKSIZE;
				return;
			}
			break;
		case _OP_LOADNULLS:
			if((pi.op == _OP_LOADNULLS && pi._arg0+pi._arg1 == i._arg0)) {

				pi._arg1 = pi._arg1 + 1;
				pi.op = _OP_LOADNULLS;
				return;
			}
            break;
		case _OP_LINE:
			if(pi.op == _OP_LINE) {
				_instructions.pop_back();
				_lineinfos.pop_back();
			}
			break;
		}
	}
	_optimization = true;
	_instructions.push_back(i);
}

SQObject SQFuncState::CreateString(const SQChar *s,SQInteger len)
{
	SQObjectPtr ns(SQString::Create(_sharedstate,s,len));
	_table(_strings)->NewSlot(ns,(SQInteger)1);
	return std::move(ns);
}

SQObject SQFuncState::CreateTable()
{
	SQObjectPtr nt(SQTable::Create(_sharedstate,0));
	_table(_strings)->NewSlot(nt,(SQInteger)1);
	return std::move(nt);
}

SQFunctionProto *SQFuncState::BuildProto()
{
	SQFunctionProto *f=SQFunctionProto::Create(_instructions.size(),
		_nliterals,_parameters.size(),_functions.size(),_outervalues.size(),
		_lineinfos.size(),_localvarinfos.size(),_defaultparams.size());

	SQObjectPtr refidx,key,val;
	SQInteger idx;

	f->_stacksize = _stacksize;
	f->_sourcename = _sourcename;
	f->_bgenerator = _bgenerator;
	f->_name = _name;

	while((idx=_table(_literals)->Next(false,refidx,key,val))!=-1) {
		f->_literals[_integer(val)]=key;
		refidx=idx;
	}

	for(SQUnsignedInteger nf = 0; nf < _functions.size(); nf++) f->_functions[nf] = _functions[nf];
	for(SQUnsignedInteger np = 0; np < _parameters.size(); np++) f->_parameters[np] = _parameters[np];
	for(SQUnsignedInteger no = 0; no < _outervalues.size(); no++) f->_outervalues[no] = _outervalues[no];
	for(SQUnsignedInteger no = 0; no < _localvarinfos.size(); no++) f->_localvarinfos[no] = _localvarinfos[no];
	for(SQUnsignedInteger no = 0; no < _lineinfos.size(); no++) f->_lineinfos[no] = _lineinfos[no];
	for(SQUnsignedInteger no = 0; no < _defaultparams.size(); no++) f->_defaultparams[no] = _defaultparams[no];

	memcpy(f->_instructions,&_instructions[0],(size_t)_instructions.size()*sizeof(SQInstruction));

	f->_varparams = _varparams;

	return f;
}

SQFuncState *SQFuncState::PushChildState(SQSharedState *ss)
{
	SQFuncState *child = (SQFuncState *)sq_malloc(sizeof(SQFuncState));
	new (child) SQFuncState(ss,this,_errfunc,_errtarget);
	_childstates.push_back(child);
	return child;
}

void SQFuncState::PopChildState()
{
	SQFuncState *child = _childstates.back();
	sq_delete(child,SQFuncState);
	_childstates.pop_back();
}

SQFuncState::~SQFuncState()
{
	while(!_childstates.empty())
	{
		PopChildState();
	}
}
