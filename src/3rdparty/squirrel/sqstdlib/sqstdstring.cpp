/* see copyright notice in squirrel.h */
#include <squirrel.h>
#include <sqstdstring.h>

#define scstrchr strchr
#define scatoi atoi
#define scstrtok strtok
#define MAX_FORMAT_LEN	20
#define MAX_WFORMAT_LEN	3
#define ADDITIONAL_FORMAT_SPACE (100*sizeof(SQChar))

static SQInteger validate_format(HSQUIRRELVM v, SQChar *fmt, const SQChar *src, SQInteger n,SQInteger &width)
{
	SQChar swidth[MAX_WFORMAT_LEN];
	SQInteger wc = 0;
	SQInteger start = n;
	fmt[0] = '%';
	while (scstrchr("-+ #0", src[n])) n++;
	while (isdigit(src[n])) {
		swidth[wc] = src[n];
		n++;
		wc++;
		if(wc>=MAX_WFORMAT_LEN)
			return sq_throwerror(v,"width format too long");
	}
	swidth[wc] = '\0';
	if(wc > 0) {
		width = atoi(swidth);
	}
	else
		width = 0;
	if (src[n] == '.') {
	    n++;

		wc = 0;
		while (isdigit(src[n])) {
			swidth[wc] = src[n];
			n++;
			wc++;
			if(wc>=MAX_WFORMAT_LEN)
				return sq_throwerror(v,"precision format too long");
		}
		swidth[wc] = '\0';
		if(wc > 0) {
			width += atoi(swidth);
		}
	}
	if (n-start > MAX_FORMAT_LEN )
		return sq_throwerror(v,"format too long");
	memcpy(&fmt[1],&src[start],((n-start)+1)*sizeof(SQChar));
	fmt[(n-start)+2] = '\0';
	return n;
}

static void __strip_l(const SQChar *str,const SQChar **start)
{
	const SQChar *t = str;
	while(((*t) != '\0') && isspace(*t)){ t++; }
	*start = t;
}

static void __strip_r(const SQChar *str,SQInteger len,const SQChar **end)
{
	if(len == 0) {
		*end = str;
		return;
	}
	const SQChar *t = &str[len-1];
	while(t != str && isspace(*t)) { t--; }
	*end = t+1;
}

static SQInteger _string_strip(HSQUIRRELVM v)
{
	const SQChar *str,*start,*end;
	sq_getstring(v,2,&str);
	SQInteger len = sq_getsize(v,2);
	__strip_l(str,&start);
	__strip_r(str,len,&end);
	sq_pushstring(v,start,end - start);
	return 1;
}

static SQInteger _string_lstrip(HSQUIRRELVM v)
{
	const SQChar *str,*start;
	sq_getstring(v,2,&str);
	__strip_l(str,&start);
	sq_pushstring(v,start,-1);
	return 1;
}

static SQInteger _string_rstrip(HSQUIRRELVM v)
{
	const SQChar *str,*end;
	sq_getstring(v,2,&str);
	SQInteger len = sq_getsize(v,2);
	__strip_r(str,len,&end);
	sq_pushstring(v,str,end - str);
	return 1;
}

static SQInteger _string_split(HSQUIRRELVM v)
{
	const SQChar *str,*seps;
	SQChar *stemp,*tok;
	sq_getstring(v,2,&str);
	sq_getstring(v,3,&seps);
	if(sq_getsize(v,3) == 0) return sq_throwerror(v,"empty separators string");
	SQInteger memsize = (sq_getsize(v,2)+1)*sizeof(SQChar);
	stemp = sq_getscratchpad(v,memsize);
	memcpy(stemp,str,memsize);
	tok = scstrtok(stemp,seps);
	sq_newarray(v,0);
	while( tok != nullptr ) {
		sq_pushstring(v,tok,-1);
		sq_arrayappend(v,-2);
		tok = scstrtok( nullptr, seps );
	}
	return 1;
}

#define SETUP_REX(v) \
	SQRex *self = nullptr; \
	sq_getinstanceup(v,1,(SQUserPointer *)&self,0);

static SQInteger _rexobj_releasehook(SQUserPointer p, SQInteger size)
{
	SQRex *self = ((SQRex *)p);
	sqstd_rex_free(self);
	return 1;
}

static SQInteger _regexp_match(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str;
	sq_getstring(v,2,&str);
	if(sqstd_rex_match(self,str) == SQTrue)
	{
		sq_pushbool(v,SQTrue);
		return 1;
	}
	sq_pushbool(v,SQFalse);
	return 1;
}

static void _addrexmatch(HSQUIRRELVM v,const SQChar *str,const SQChar *begin,const SQChar *end)
{
	sq_newtable(v);
	sq_pushstring(v,"begin",-1);
	sq_pushinteger(v,begin - str);
	sq_rawset(v,-3);
	sq_pushstring(v,"end",-1);
	sq_pushinteger(v,end - str);
	sq_rawset(v,-3);
}

static SQInteger _regexp_search(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str,*begin,*end;
	SQInteger start = 0;
	sq_getstring(v,2,&str);
	if(sq_gettop(v) > 2) sq_getinteger(v,3,&start);
	if(sqstd_rex_search(self,str+start,&begin,&end) == SQTrue) {
		_addrexmatch(v,str,begin,end);
		return 1;
	}
	return 0;
}

static SQInteger _regexp_capture(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str,*begin,*end;
	SQInteger start = 0;
	sq_getstring(v,2,&str);
	if(sq_gettop(v) > 2) sq_getinteger(v,3,&start);
	if(sqstd_rex_search(self,str+start,&begin,&end) == SQTrue) {
		SQInteger n = sqstd_rex_getsubexpcount(self);
		SQRexMatch match;
		sq_newarray(v,0);
		for(SQInteger i = 0;i < n; i++) {
			sqstd_rex_getsubexp(self,i,&match);
			if(match.len > 0)
				_addrexmatch(v,str,match.begin,match.begin+match.len);
			else
				_addrexmatch(v,str,str,str); //empty match
			sq_arrayappend(v,-2);
		}
		return 1;
	}
	return 0;
}

static SQInteger _regexp_subexpcount(HSQUIRRELVM v)
{
	SETUP_REX(v);
	sq_pushinteger(v,sqstd_rex_getsubexpcount(self));
	return 1;
}

static SQInteger _regexp_constructor(HSQUIRRELVM v)
{
	const SQChar *error,*pattern;
	sq_getstring(v,2,&pattern);
	SQRex *rex = sqstd_rex_compile(pattern,&error);
	if(!rex) return sq_throwerror(v,error);
	sq_setinstanceup(v,1,rex);
	sq_setreleasehook(v,1,_rexobj_releasehook);
	return 0;
}

static SQInteger _regexp__typeof(HSQUIRRELVM v)
{
	sq_pushstring(v,"regexp",-1);
	return 1;
}

#define _DECL_REX_FUNC(name,nparams,pmask) {#name,_regexp_##name,nparams,pmask}
static SQRegFunction rexobj_funcs[]={
	_DECL_REX_FUNC(constructor,2,".s"),
	_DECL_REX_FUNC(search,-2,"xsn"),
	_DECL_REX_FUNC(match,2,"xs"),
	_DECL_REX_FUNC(capture,-2,"xsn"),
	_DECL_REX_FUNC(subexpcount,1,"x"),
	_DECL_REX_FUNC(_typeof,1,"x"),
	{0,0,0,0}
};

#define _DECL_FUNC(name,nparams,pmask) {#name,_string_##name,nparams,pmask}
static SQRegFunction stringlib_funcs[]={
	_DECL_FUNC(format,-2,".s"),
	_DECL_FUNC(strip,2,".s"),
	_DECL_FUNC(lstrip,2,".s"),
	_DECL_FUNC(rstrip,2,".s"),
	_DECL_FUNC(split,3,".ss"),
	{0,0,0,0}
};


SQInteger sqstd_register_stringlib(HSQUIRRELVM v)
{
	sq_pushstring(v,"regexp",-1);
	sq_newclass(v,SQFalse);
	SQInteger i = 0;
	while(rexobj_funcs[i].name != 0) {
		SQRegFunction &f = rexobj_funcs[i];
		sq_pushstring(v,f.name,-1);
		sq_newclosure(v,f.f,0);
		sq_setparamscheck(v,f.nparamscheck,f.typemask);
		sq_setnativeclosurename(v,-1,f.name);
		sq_createslot(v,-3);
		i++;
	}
	sq_createslot(v,-3);

	i = 0;
	while(stringlib_funcs[i].name!=0)
	{
		sq_pushstring(v,stringlib_funcs[i].name,-1);
		sq_newclosure(v,stringlib_funcs[i].f,0);
		sq_setparamscheck(v,stringlib_funcs[i].nparamscheck,stringlib_funcs[i].typemask);
		sq_setnativeclosurename(v,-1,stringlib_funcs[i].name);
		sq_createslot(v,-3);
		i++;
	}
	return 1;
}
