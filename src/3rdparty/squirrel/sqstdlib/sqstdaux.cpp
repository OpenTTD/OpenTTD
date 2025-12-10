/* see copyright notice in squirrel.h */

#include "../../../stdafx.h"

#include <squirrel.h>
#include <sqstdaux.h>

#include "../../fmt/format.h"
#include "../../../safeguards.h"

void sqstd_printcallstack(HSQUIRRELVM v)
{
	SQPRINTFUNCTION pf = sq_getprintfunc(v);
	if(pf) {
		SQStackInfos si;
		SQInteger i;
		SQBool b;
		SQFloat f;
		SQInteger level=1; //1 is to skip this function that is level 0
		SQInteger seq=0;
		pf(v,"\n");
		pf(v,"CALLSTACK\n");
		while(SQ_SUCCEEDED(sq_stackinfos(v,level,&si)))
		{
			std::string_view fn="unknown";
			std::string_view src="unknown";
			if(!si.funcname.empty())fn=si.funcname;
			if(!si.source.empty()) {
				/* We don't want to bother users with absolute paths to all AI files.
				 * Since the path only reaches NoAI code in a formatted string we have
				 * to strip it here. Let's hope nobody installs openttd in a subdirectory
				 * of a directory named /ai/. */
				auto p = si.source.find("\\ai\\");
				if (p == std::string_view::npos) p = si.source.find("/ai/");
				src = (p == std::string_view::npos) ? si.source : si.source.substr(p + 4);
			}
			pf(v,fmt::format("*FUNCTION [{}()] {} line [{}]\n",fn,src,si.line));
			level++;
		}
		level=0;
		pf(v,"\n");
		pf(v,"LOCALS\n");

		for(level=0;level<10;level++){
			seq=0;
			std::optional<std::string_view> opt;
			while ((opt = sq_getlocal(v,level,seq)).has_value()) {
				std::string_view name = *opt;
				seq++;
				switch(sq_gettype(v,-1))
				{
				case OT_NULL:
					pf(v,fmt::format("[{}] NULL\n",name));
					break;
				case OT_INTEGER:
					sq_getinteger(v,-1,&i);
					pf(v,fmt::format("[{}] {}\n",name,i));
					break;
				case OT_FLOAT:
					sq_getfloat(v,-1,&f);
					pf(v,fmt::format("[{}] {:14g}\n",name,f));
					break;
				case OT_USERPOINTER:
					pf(v,fmt::format("[{}] USERPOINTER\n",name));
					break;
				case OT_STRING: {
					std::string_view view;
					sq_getstring(v,-1,view);
					pf(v,fmt::format("[{}] \"{}\"\n",name,view));
					break;
				}
				case OT_TABLE:
					pf(v,fmt::format("[{}] TABLE\n",name));
					break;
				case OT_ARRAY:
					pf(v,fmt::format("[{}] ARRAY\n",name));
					break;
				case OT_CLOSURE:
					pf(v,fmt::format("[{}] CLOSURE\n",name));
					break;
				case OT_NATIVECLOSURE:
					pf(v,fmt::format("[{}] NATIVECLOSURE\n",name));
					break;
				case OT_GENERATOR:
					pf(v,fmt::format("[{}] GENERATOR\n",name));
					break;
				case OT_USERDATA:
					pf(v,fmt::format("[{}] USERDATA\n",name));
					break;
				case OT_THREAD:
					pf(v,fmt::format("[{}] THREAD\n",name));
					break;
				case OT_CLASS:
					pf(v,fmt::format("[{}] CLASS\n",name));
					break;
				case OT_INSTANCE:
					pf(v,fmt::format("[{}] INSTANCE\n",name));
					break;
				case OT_WEAKREF:
					pf(v,fmt::format("[{}] WEAKREF\n",name));
					break;
				case OT_BOOL:{
					sq_getbool(v,-1,&b);
					pf(v,fmt::format("[{}] {}\n",name,b?"true":"false"));
							 }
					break;
				default: assert(0); break;
				}
				sq_pop(v,1);
			}
		}
	}
}

static SQInteger _sqstd_aux_printerror(HSQUIRRELVM v)
{
	SQPRINTFUNCTION pf = sq_getprintfunc(v);
	if(pf) {
		std::string_view error;
		if(sq_gettop(v)>=1) {
			pf(v,"\n");
			if(SQ_SUCCEEDED(sq_getstring(v,2,error))) {
				pf(v,fmt::format("AN ERROR HAS OCCURRED [{}]\n",error));
			}
			else{
				pf(v,"AN ERROR HAS OCCURRED [unknown]\n");
			}
			sqstd_printcallstack(v);
		}
	}
	return 0;
}

void _sqstd_compiler_error(HSQUIRRELVM v,std::string_view sErr,std::string_view sSource,SQInteger line,SQInteger column)
{
	SQPRINTFUNCTION pf = sq_getprintfunc(v);
	if(pf) {
		pf(v,fmt::format("{} line = ({}) column = ({}) : error {}\n",sSource,line,column,sErr));
	}
}

void sqstd_seterrorhandlers(HSQUIRRELVM v)
{
	sq_setcompilererrorhandler(v,_sqstd_compiler_error);
	sq_newclosure(v,_sqstd_aux_printerror,0);
	sq_seterrorhandler(v);
}
