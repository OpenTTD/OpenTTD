/*
	see copyright notice in squirrel.h
*/
#include "sqpcheader.h"
#include "sqcompiler.h"
#include "sqfuncproto.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqopcodes.h"
#include "sqfuncstate.h"

#ifdef _DEBUG_DUMP
SQInstructionDesc g_InstrDesc[]={
	{_SC("_OP_LINE")},
	{_SC("_OP_LOAD")},
	{_SC("_OP_LOADINT")},
	{_SC("_OP_LOADFLOAT")},
	{_SC("_OP_DLOAD")},
	{_SC("_OP_TAILCALL")},
	{_SC("_OP_CALL")},
	{_SC("_OP_PREPCALL")},
	{_SC("_OP_PREPCALLK")},
	{_SC("_OP_GETK")},
	{_SC("_OP_MOVE")},
	{_SC("_OP_NEWSLOT")},
	{_SC("_OP_DELETE")},
	{_SC("_OP_SET")},
	{_SC("_OP_GET")},
	{_SC("_OP_EQ")},
	{_SC("_OP_NE")},
	{_SC("_OP_ARITH")},
	{_SC("_OP_BITW")},
	{_SC("_OP_RETURN")},
	{_SC("_OP_LOADNULLS")},
	{_SC("_OP_LOADROOTTABLE")},
	{_SC("_OP_LOADBOOL")},
	{_SC("_OP_DMOVE")},
	{_SC("_OP_JMP")},
	{_SC("_OP_JNZ")},
	{_SC("_OP_JZ")},
	{_SC("_OP_LOADFREEVAR")},
	{_SC("_OP_VARGC")},
	{_SC("_OP_GETVARGV")},
	{_SC("_OP_NEWTABLE")},
	{_SC("_OP_NEWARRAY")},
	{_SC("_OP_APPENDARRAY")},
	{_SC("_OP_GETPARENT")},
	{_SC("_OP_COMPARITH")},
	{_SC("_OP_COMPARITHL")},
	{_SC("_OP_INC")},
	{_SC("_OP_INCL")},
	{_SC("_OP_PINC")},
	{_SC("_OP_PINCL")},
	{_SC("_OP_CMP")},
	{_SC("_OP_EXISTS")},
	{_SC("_OP_INSTANCEOF")},
	{_SC("_OP_AND")},
	{_SC("_OP_OR")},
	{_SC("_OP_NEG")},
	{_SC("_OP_NOT")},
	{_SC("_OP_BWNOT")},
	{_SC("_OP_CLOSURE")},
	{_SC("_OP_YIELD")},
	{_SC("_OP_RESUME")},
	{_SC("_OP_FOREACH")},
	{_SC("_OP_POSTFOREACH")},
	{_SC("_OP_DELEGATE")},
	{_SC("_OP_CLONE")},
	{_SC("_OP_TYPEOF")},
	{_SC("_OP_PUSHTRAP")},
	{_SC("_OP_POPTRAP")},
	{_SC("_OP_THROW")},
	{_SC("_OP_CLASS")},
	{_SC("_OP_NEWSLOTA")},
	{_SC("_OP_SCOPE_END")}
};
#endif
void DumpLiteral(SQObjectPtr &o)
{
	switch(type(o)){
		case OT_STRING:	scprintf(_SC("\"%s\""),_stringval(o));break;
		case OT_FLOAT: scprintf(_SC("{%f}"),_float(o));break;
#if defined(_SQ64)
		case OT_INTEGER: scprintf(_SC("{%ld}"),_integer(o));break;
#else
		case OT_INTEGER: scprintf(_SC("{%d}"),_integer(o));break;
#endif
		case OT_BOOL: scprintf(_SC("%s"),_integer(o)?_SC("true"):_SC("false"));break;
		default: scprintf(_SC("(%s %p)"),GetTypeName(o),(void*)_rawval(o));break; break; //shut up compiler
	}
}

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
	scprintf(_SC("SQInstruction sizeof %d\n"),sizeof(SQInstruction));
	scprintf(_SC("SQObject sizeof %d\n"),sizeof(SQObject));
	scprintf(_SC("--------------------------------------------------------------------\n"));
	scprintf(_SC("*****FUNCTION [%s]\n"),type(func->_name)==OT_STRING?_stringval(func->_name):_SC("unknown"));
	scprintf(_SC("-----LITERALS\n"));
	SQObjectPtr refidx,key,val;
	SQInteger idx;
	SQObjectPtrVec templiterals;
	templiterals.resize(_nliterals);
	while((idx=_table(_literals)->Next(false,refidx,key,val))!=-1) {
		refidx=idx;
		templiterals[_integer(val)]=key;
	}
	for(i=0;i<templiterals.size();i++){
		scprintf(_SC("[%d] "),n);
		DumpLiteral(templiterals[i]);
		scprintf(_SC("\n"));
		n++;
	}
	scprintf(_SC("-----PARAMS\n"));
	if(_varparams)
		scprintf(_SC("<<VARPARAMS>>\n"));
	n=0;
	for(i=0;i<_parameters.size();i++){
		scprintf(_SC("[%d] "),n);
		DumpLiteral(_parameters[i]);
		scprintf(_SC("\n"));
		n++;
	}
	scprintf(_SC("-----LOCALS\n"));
	for(si=0;si<func->_nlocalvarinfos;si++){
		SQLocalVarInfo lvi=func->_localvarinfos[si];
		scprintf(_SC("[%d] %s \t%d %d\n"),lvi._pos,_stringval(lvi._name),lvi._start_op,lvi._end_op);
		n++;
	}
	scprintf(_SC("-----LINE INFO\n"));
	for(i=0;i<_lineinfos.size();i++){
		SQLineInfo li=_lineinfos[i];
		scprintf(_SC("op [%d] line [%d] \n"),li._op,li._line);
		n++;
	}
	scprintf(_SC("-----dump\n"));
	n=0;
	for(i=0;i<_instructions.size();i++){
		SQInstruction &inst=_instructions[i];
		if(inst.op==_OP_LOAD || inst.op==_OP_DLOAD || inst.op==_OP_PREPCALLK || inst.op==_OP_GETK ){

			SQInteger lidx = inst._arg1;
			scprintf(_SC("[%03d] %15s %d "),n,g_InstrDesc[inst.op].name,inst._arg0);
			if(lidx >= 0xFFFFFFFF)
				scprintf(_SC("null"));
			else {
				SQInteger refidx;
				SQObjectPtr val,key,refo;
				while(((refidx=_table(_literals)->Next(false,refo,key,val))!= -1) && (_integer(val) != lidx)) {
					refo = refidx;
				}
				DumpLiteral(key);
			}
			if(inst.op != _OP_DLOAD) {
				scprintf(_SC(" %d %d \n"),inst._arg2,inst._arg3);
			}
			else {
				scprintf(_SC(" %d "),inst._arg2);
				lidx = inst._arg3;
				if(lidx >= 0xFFFFFFFF)
					scprintf(_SC("null"));
				else {
					SQInteger refidx;
					SQObjectPtr val,key,refo;
					while(((refidx=_table(_literals)->Next(false,refo,key,val))!= -1) && (_integer(val) != lidx)) {
						refo = refidx;
				}
				DumpLiteral(key);
				scprintf(_SC("\n"));
			}
			}
		}
		else if(inst.op==_OP_LOADFLOAT) {
			scprintf(_SC("[%03d] %15s %d %f %d %d\n"),n,g_InstrDesc[inst.op].name,inst._arg0,*((SQFloat*)&inst._arg1),inst._arg2,inst._arg3);
		}
		else if(inst.op==_OP_ARITH){
			scprintf(_SC("[%03d] %15s %d %d %d %c\n"),n,g_InstrDesc[inst.op].name,inst._arg0,inst._arg1,inst._arg2,inst._arg3);
		}
		else
			scprintf(_SC("[%03d] %15s %d %d %d %d\n"),n,g_InstrDesc[inst.op].name,inst._arg0,inst._arg1,inst._arg2,inst._arg3);
		n++;
	}
	scprintf(_SC("-----\n"));
	scprintf(_SC("stack size[%d]\n"),func->_stacksize);
	scprintf(_SC("--------------------------------------------------------------------\n\n"));
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
			Error(_SC("internal compiler error: too many literals"));
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
		if(_stacksize>MAX_FUNC_STACKSIZE) Error(_SC("internal compiler error: too many locals"));
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
				pi._arg1 = pi._arg1;
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
				pi._arg1 = pi._arg1;
				pi._arg2 = i._arg2;
				pi._arg3 = i._arg3;
				return;
			}
			break;
		case _OP_APPENDARRAY:
			if(pi.op == _OP_LOAD && pi._arg0 == i._arg1 && (!IsLocal(pi._arg0))){
				pi.op = _OP_APPENDARRAY;
				pi._arg0 = i._arg0;
				pi._arg1 = pi._arg1;
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
				pi._arg1 = pi._arg1;
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
	return ns;
}

SQObject SQFuncState::CreateTable()
{
	SQObjectPtr nt(SQTable::Create(_sharedstate,0));
	_table(_strings)->NewSlot(nt,(SQInteger)1);
	return nt;
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

	memcpy(f->_instructions,&_instructions[0],_instructions.size()*sizeof(SQInstruction));

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
	while(_childstates.size() > 0)
	{
		PopChildState();
	}
}
