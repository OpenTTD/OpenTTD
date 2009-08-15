/*	see copyright notice in squirrel.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#include <conio.h>
#endif
#include <squirrel.h>
#include <sqstdblob.h>
#include <sqstdsystem.h>
#include <sqstdio.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdaux.h>

#ifdef SQUNICODE
#define scfprintf fwprintf
#define scfopen	_wfopen
#define scvprintf vwprintf
#else
#define scfprintf fprintf
#define scfopen	fopen
#define scvprintf vprintf
#endif


void PrintVersionInfos();

#if defined(_MSC_VER) && defined(_DEBUG)
int MemAllocHook( int allocType, void *userData, size_t size, int blockType,
   long requestNumber, const unsigned char *filename, int lineNumber)
{
//	if(requestNumber==585)_asm int 3;
	return 1;
}
#endif


SQInteger quit(HSQUIRRELVM v)
{
	int *done;
	sq_getuserpointer(v,-1,(SQUserPointer*)&done);
	*done=1;
	return 0;
}

void printfunc(HSQUIRRELVM v,const SQChar *s,...)
{
	va_list vl;
	va_start(vl, s);
	scvprintf( s, vl);
	va_end(vl);
}

void PrintVersionInfos()
{
	scfprintf(stdout,_SC("%s %s (%d bits)\n"),SQUIRREL_VERSION,SQUIRREL_COPYRIGHT,sizeof(SQInteger)*8);
	if(sizeof(SQFloat) != sizeof(float)) {
		scfprintf(stdout,_SC("[%d bits floats]\n"),sizeof(SQFloat)*8);
	}
}

void PrintUsage()
{
	scfprintf(stderr,_SC("usage: sq <options> <scriptpath [args]>.\n")
		_SC("Available options are:\n")
		_SC("   -c              compiles the file to bytecode(default output 'out.cnut')\n")
		_SC("   -o              specifies output file for the -c option\n")
		_SC("   -c              compiles only\n")
		_SC("   -d              generates debug infos\n")
		_SC("   -v              displays version infos\n")
		_SC("   -h              prints help\n"));
}

#define _INTERACTIVE 0
#define _DONE 2
//<<FIXME>> this func is a mess
int getargs(HSQUIRRELVM v,int argc, char* argv[])
{
	int i;
	int compiles_only = 0;
	static SQChar temp[500];
	const SQChar *ret=NULL;
	char * output = NULL;
	int lineinfo=0;
	if(argc>1)
	{
		int arg=1,exitloop=0;
		while(arg < argc && !exitloop)
		{

			if(argv[arg][0]=='-')
			{
				switch(argv[arg][1])
				{
				case 'd': //DEBUG(debug infos)
					sq_enabledebuginfo(v,1);
					break;
				case 'c':
					compiles_only = 1;
					break;
				case 'o':
					if(arg < argc) {
						arg++;
						output = argv[arg];
					}
					break;
				case 'v':
					PrintVersionInfos();
					return _DONE;

				case 'h':
					PrintVersionInfos();
					PrintUsage();
					return _DONE;
				default:
					PrintVersionInfos();
					scprintf(_SC("unknown prameter '-%c'\n"),argv[arg][1]);
					PrintUsage();
					return _DONE;
				}
			}else break;
			arg++;
		}

		// src file

		if(arg<argc) {
			const SQChar *filename=NULL;
#ifdef SQUNICODE
			mbstowcs(temp,argv[arg],strlen(argv[arg]));
			filename=temp;
#else
			filename=argv[arg];
#endif

			arg++;
			sq_pushroottable(v);
			sq_pushstring(v,_SC("ARGS"),-1);
			sq_newarray(v,0);
			for(i=arg;i<argc;i++)
			{
				const SQChar *a;
#ifdef SQUNICODE
				int alen=(int)strlen(argv[i]);
				a=sq_getscratchpad(v,(int)(alen*sizeof(SQChar)));
				mbstowcs(sq_getscratchpad(v,-1),argv[i],alen);
				sq_getscratchpad(v,-1)[alen] = _SC('\0');
#else
				a=argv[i];
#endif
				sq_pushstring(v,a,-1);

				sq_arrayappend(v,-2);
			}
			sq_createslot(v,-3);
			sq_pop(v,1);
			if(compiles_only) {
				if(SQ_SUCCEEDED(sqstd_loadfile(v,filename,SQTrue))){
					SQChar *outfile = _SC("out.cnut");
					if(output) {
#ifdef SQUNICODE
						int len = (int)(strlen(output)+1);
						mbstowcs(sq_getscratchpad(v,len*sizeof(SQChar)),output,len);
						outfile = sq_getscratchpad(v,-1);
#else
						outfile = output;
#endif
					}
					if(SQ_SUCCEEDED(sqstd_writeclosuretofile(v,outfile)))
						return _DONE;
				}
			}
			else {
				if(SQ_SUCCEEDED(sqstd_dofile(v,filename,SQFalse,SQTrue))) {
					return _DONE;
				}
			}
			//if this point is reached an error occured
			{
				const SQChar *err;
				sq_getlasterror(v);
				if(SQ_SUCCEEDED(sq_getstring(v,-1,&err))) {
					scprintf(_SC("Error [%s]\n"),err);
					return _DONE;
				}
			}

		}
	}

	return _INTERACTIVE;
}

void Interactive(HSQUIRRELVM v)
{

#define MAXINPUT 1024
	SQChar buffer[MAXINPUT];
	SQInteger blocks =0;
	SQInteger string=0;
	SQInteger retval=0;
	SQInteger done=0;
	PrintVersionInfos();

	sq_pushroottable(v);
	sq_pushstring(v,_SC("quit"),-1);
	sq_pushuserpointer(v,&done);
	sq_newclosure(v,quit,1);
	sq_setparamscheck(v,1,NULL);
	sq_createslot(v,-3);
	sq_pop(v,1);

    while (!done)
	{
		SQInteger i = 0;
		scprintf(_SC("\nsq>"));
		for(;;) {
			int c;
			if(done)return;
			c = getchar();
			if (c == _SC('\n')) {
				if (i>0 && buffer[i-1] == _SC('\\'))
				{
					buffer[i-1] = _SC('\n');
				}
				else if(blocks==0)break;
				buffer[i++] = _SC('\n');
			}
			else if (c==_SC('}')) {blocks--; buffer[i++] = (SQChar)c;}
			else if(c==_SC('{') && !string){
					blocks++;
					buffer[i++] = (SQChar)c;
			}
			else if(c==_SC('"') || c==_SC('\'')){
					string=!string;
					buffer[i++] = (SQChar)c;
			}
			else if (i >= MAXINPUT-1) {
				scfprintf(stderr, _SC("sq : input line too long\n"));
				break;
			}
			else{
				buffer[i++] = (SQChar)c;
			}
		}
		buffer[i] = _SC('\0');

		if(buffer[0]==_SC('=')){
			scsprintf(sq_getscratchpad(v,MAXINPUT),_SC("return (%s)"),&buffer[1]);
			memcpy(buffer,sq_getscratchpad(v,-1),(scstrlen(sq_getscratchpad(v,-1))+1)*sizeof(SQChar));
			retval=1;
		}
		i=scstrlen(buffer);
		if(i>0){
			SQInteger oldtop=sq_gettop(v);
			if(SQ_SUCCEEDED(sq_compilebuffer(v,buffer,i,_SC("interactive console"),SQTrue))){
				sq_pushroottable(v);
				if(SQ_SUCCEEDED(sq_call(v,1,retval,SQTrue)) &&	retval){
					scprintf(_SC("\n"));
					sq_pushroottable(v);
					sq_pushstring(v,_SC("print"),-1);
					sq_get(v,-2);
					sq_pushroottable(v);
					sq_push(v,-4);
					sq_call(v,2,SQFalse,SQTrue);
					retval=0;
					scprintf(_SC("\n"));
				}
			}

			sq_settop(v,oldtop);
		}
	}
}

int main(int argc, char* argv[])
{
	HSQUIRRELVM v;

	const SQChar *filename=NULL;
#if defined(_MSC_VER) && defined(_DEBUG)
	_CrtSetAllocHook(MemAllocHook);
#endif

	v=sq_open(1024);
	sq_setprintfunc(v,printfunc);

	sq_pushroottable(v);

	sqstd_register_bloblib(v);
	sqstd_register_iolib(v);
	sqstd_register_systemlib(v);
	sqstd_register_mathlib(v);
	sqstd_register_stringlib(v);

	//aux library
	//sets error handlers
	sqstd_seterrorhandlers(v);

	//gets arguments
	switch(getargs(v,argc,argv))
	{
	case _INTERACTIVE:
		Interactive(v);
		break;
	case _DONE:
	default:
		break;
	}

	sq_close(v);

#if defined(_MSC_VER) && defined(_DEBUG)
	_getch();
	_CrtMemDumpAllObjectsSince( NULL );
#endif
	return 0;
}

