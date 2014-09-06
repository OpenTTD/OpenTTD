/* see copyright notice in squirrel.h */
#include <squirrel.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sqstdsystem.h>

#define scgetenv getenv
#define scsystem system
#define scasctime asctime
#define scremove remove
#define screname rename

static SQInteger _system_getenv(HSQUIRRELVM v)
{
	const SQChar *s;
	if(SQ_SUCCEEDED(sq_getstring(v,2,&s))){
        sq_pushstring(v,scgetenv(s),-1);
		return 1;
	}
	return 0;
}


static SQInteger _system_system(HSQUIRRELVM v)
{
	const SQChar *s;
	if(SQ_SUCCEEDED(sq_getstring(v,2,&s))){
		sq_pushinteger(v,scsystem(s));
		return 1;
	}
	return sq_throwerror(v,"wrong param");
}


static SQInteger _system_clock(HSQUIRRELVM v)
{
	sq_pushfloat(v,((SQFloat)clock())/(SQFloat)CLOCKS_PER_SEC);
	return 1;
}

static SQInteger _system_time(HSQUIRRELVM v)
{
	time_t t;
	time(&t);
	sq_pushinteger(v,*((SQInteger *)&t));
	return 1;
}

static SQInteger _system_remove(HSQUIRRELVM v)
{
	const SQChar *s;
	sq_getstring(v,2,&s);
	if(scremove(s)==-1)
		return sq_throwerror(v,"remove() failed");
	return 0;
}

static SQInteger _system_rename(HSQUIRRELVM v)
{
	const SQChar *oldn,*newn;
	sq_getstring(v,2,&oldn);
	sq_getstring(v,3,&newn);
	if(screname(oldn,newn)==-1)
		return sq_throwerror(v,"rename() failed");
	return 0;
}

static void _set_integer_slot(HSQUIRRELVM v,const SQChar *name,SQInteger val)
{
	sq_pushstring(v,name,-1);
	sq_pushinteger(v,val);
	sq_rawset(v,-3);
}

static SQInteger _system_date(HSQUIRRELVM v)
{
	time_t t;
	SQInteger it;
	SQInteger format = 'l';
	if(sq_gettop(v) > 1) {
		sq_getinteger(v,2,&it);
		t = it;
		if(sq_gettop(v) > 2) {
			sq_getinteger(v,3,(SQInteger*)&format);
		}
	}
	else {
		time(&t);
	}
	tm *date;
    if(format == 'u')
		date = gmtime(&t);
	else
		date = localtime(&t);
	if(!date)
		return sq_throwerror(v,"crt api failure");
	sq_newtable(v);
	_set_integer_slot(v, "sec", date->tm_sec);
    _set_integer_slot(v, "min", date->tm_min);
    _set_integer_slot(v, "hour", date->tm_hour);
    _set_integer_slot(v, "day", date->tm_mday);
    _set_integer_slot(v, "month", date->tm_mon);
    _set_integer_slot(v, "year", date->tm_year+1900);
    _set_integer_slot(v, "wday", date->tm_wday);
    _set_integer_slot(v, "yday", date->tm_yday);
	return 1;
}



#define _DECL_FUNC(name,nparams,pmask) {#name,_system_##name,nparams,pmask}
static SQRegFunction systemlib_funcs[]={
	_DECL_FUNC(getenv,2,".s"),
	_DECL_FUNC(system,2,".s"),
	_DECL_FUNC(clock,1,NULL),
	_DECL_FUNC(time,1,NULL),
	_DECL_FUNC(date,-1,".nn"),
	_DECL_FUNC(remove,2,".s"),
	_DECL_FUNC(rename,3,".ss"),
	{0,0,0,0}
};


SQInteger sqstd_register_systemlib(HSQUIRRELVM v)
{
	SQInteger i=0;
	while(systemlib_funcs[i].name!=0)
	{
		sq_pushstring(v,systemlib_funcs[i].name,-1);
		sq_newclosure(v,systemlib_funcs[i].f,0);
		sq_setparamscheck(v,systemlib_funcs[i].nparamscheck,systemlib_funcs[i].typemask);
		sq_setnativeclosurename(v,-1,systemlib_funcs[i].name);
		sq_createslot(v,-3);
		i++;
	}
	return 1;
}
