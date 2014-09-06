/*	see copyright notice in squirrel.h */
#ifndef _SQSTDIO_H_
#define _SQSTDIO_H_

#define SQSTD_STREAM_TYPE_TAG 0x80000000

struct SQStream {
	virtual ~SQStream() {}
	virtual SQInteger Read(void *buffer, SQInteger size) = 0;
	virtual SQInteger Write(void *buffer, SQInteger size) = 0;
	virtual SQInteger Flush() = 0;
	virtual SQInteger Tell() = 0;
	virtual SQInteger Len() = 0;
	virtual SQInteger Seek(SQInteger offset, SQInteger origin) = 0;
	virtual bool IsValid() = 0;
	virtual bool EOS() = 0;
};

#define SQ_SEEK_CUR 0
#define SQ_SEEK_END 1
#define SQ_SEEK_SET 2

typedef void* SQFILE;

SQFILE sqstd_fopen(const SQChar *,const SQChar *);
SQInteger sqstd_fread(SQUserPointer, SQInteger, SQInteger, SQFILE);
SQInteger sqstd_fwrite(const SQUserPointer, SQInteger, SQInteger, SQFILE);
SQInteger sqstd_fseek(SQFILE , SQInteger , SQInteger);
SQInteger sqstd_ftell(SQFILE);
SQInteger sqstd_fflush(SQFILE);
SQInteger sqstd_fclose(SQFILE);
SQInteger sqstd_feof(SQFILE);

SQRESULT sqstd_createfile(HSQUIRRELVM v, SQFILE file,SQBool own);
SQRESULT sqstd_getfile(HSQUIRRELVM v, SQInteger idx, SQFILE *file);

//compiler helpers
SQRESULT sqstd_loadfile(HSQUIRRELVM v,const SQChar *filename,SQBool printerror);
SQRESULT sqstd_dofile(HSQUIRRELVM v,const SQChar *filename,SQBool retval,SQBool printerror);
SQRESULT sqstd_writeclosuretofile(HSQUIRRELVM v,const SQChar *filename);

SQRESULT sqstd_register_iolib(HSQUIRRELVM v);

#endif /*_SQSTDIO_H_*/

