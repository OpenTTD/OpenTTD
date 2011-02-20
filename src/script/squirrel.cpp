/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file squirrel.cpp the implementation of the Squirrel class. It handles all Squirrel-stuff and gives a nice API back to work with. */

#include <stdarg.h>
#include "../stdafx.h"
#include "../debug.h"
#include "squirrel_std.hpp"
#include "../fileio_func.h"
#include <sqstdaux.h>
#include <../squirrel/sqpcheader.h>
#include <../squirrel/sqvm.h>

void Squirrel::CompileError(HSQUIRRELVM vm, const SQChar *desc, const SQChar *source, SQInteger line, SQInteger column)
{
	SQChar buf[1024];

#ifdef _SQ64
	scsnprintf(buf, lengthof(buf), _SC("Error %s:%ld/%ld: %s"), source, line, column, desc);
#else
	scsnprintf(buf, lengthof(buf), _SC("Error %s:%d/%d: %s"), source, line, column, desc);
#endif

	/* Check if we have a custom print function */
	Squirrel *engine = (Squirrel *)sq_getforeignptr(vm);
	engine->crashed = true;
	SQPrintFunc *func = engine->print_func;
	if (func == NULL) {
		scfprintf(stderr, _SC("%s"), buf);
	} else {
		(*func)(true, buf);
	}
}

void Squirrel::ErrorPrintFunc(HSQUIRRELVM vm, const SQChar *s, ...)
{
	va_list arglist;
	SQChar buf[1024];

	va_start(arglist, s);
	scvsnprintf(buf, lengthof(buf), s, arglist);
	va_end(arglist);

	/* Check if we have a custom print function */
	SQPrintFunc *func = ((Squirrel *)sq_getforeignptr(vm))->print_func;
	if (func == NULL) {
		scfprintf(stderr, _SC("%s"), buf);
	} else {
		(*func)(true, buf);
	}
}

void Squirrel::RunError(HSQUIRRELVM vm, const SQChar *error)
{
	/* Set the print function to something that prints to stderr */
	SQPRINTFUNCTION pf = sq_getprintfunc(vm);
	sq_setprintfunc(vm, &Squirrel::ErrorPrintFunc);

	/* Check if we have a custom print function */
	SQChar buf[1024];
	scsnprintf(buf, lengthof(buf), _SC("Your script made an error: %s\n"), error);
	Squirrel *engine = (Squirrel *)sq_getforeignptr(vm);
	SQPrintFunc *func = engine->print_func;
	if (func == NULL) {
		scfprintf(stderr, _SC("%s"), buf);
	} else {
		(*func)(true, buf);
	}

	/* Print below the error the stack, so the users knows what is happening */
	sqstd_printcallstack(vm);
	/* Reset the old print function */
	sq_setprintfunc(vm, pf);
}

SQInteger Squirrel::_RunError(HSQUIRRELVM vm)
{
	const SQChar *sErr = 0;

	if (sq_gettop(vm) >= 1) {
		if (SQ_SUCCEEDED(sq_getstring(vm, -1, &sErr))) {
			Squirrel::RunError(vm, sErr);
			return 0;
		}
	}

	Squirrel::RunError(vm, _SC("unknown error"));
	return 0;
}

void Squirrel::PrintFunc(HSQUIRRELVM vm, const SQChar *s, ...)
{
	va_list arglist;
	SQChar buf[1024];

	va_start(arglist, s);
	scvsnprintf(buf, lengthof(buf) - 2, s, arglist);
	va_end(arglist);
	scstrcat(buf, _SC("\n"));

	/* Check if we have a custom print function */
	SQPrintFunc *func = ((Squirrel *)sq_getforeignptr(vm))->print_func;
	if (func == NULL) {
		scprintf(_SC("%s"), buf);
	} else {
		(*func)(false, buf);
	}
}

void Squirrel::AddMethod(const char *method_name, SQFUNCTION proc, uint nparam, const char *params, void *userdata, int size)
{
	sq_pushstring(this->vm, OTTD2SQ(method_name), -1);

	if (size != 0) {
		void *ptr = sq_newuserdata(vm, size);
		memcpy(ptr, userdata, size);
	}

	sq_newclosure(this->vm, proc, size != 0 ? 1 : 0);
	if (nparam != 0) sq_setparamscheck(this->vm, nparam, OTTD2SQ(params));
	sq_setnativeclosurename(this->vm, -1, OTTD2SQ(method_name));
	sq_newslot(this->vm, -3, SQFalse);
}

void Squirrel::AddConst(const char *var_name, int value)
{
	sq_pushstring(this->vm, OTTD2SQ(var_name), -1);
	sq_pushinteger(this->vm, value);
	sq_newslot(this->vm, -3, SQTrue);
}

void Squirrel::AddConst(const char *var_name, bool value)
{
	sq_pushstring(this->vm, OTTD2SQ(var_name), -1);
	sq_pushbool(this->vm, value);
	sq_newslot(this->vm, -3, SQTrue);
}

void Squirrel::AddClassBegin(const char *class_name)
{
	sq_pushroottable(this->vm);
	sq_pushstring(this->vm, OTTD2SQ(class_name), -1);
	sq_newclass(this->vm, SQFalse);
}

void Squirrel::AddClassBegin(const char *class_name, const char *parent_class)
{
	sq_pushroottable(this->vm);
	sq_pushstring(this->vm, OTTD2SQ(class_name), -1);
	sq_pushstring(this->vm, OTTD2SQ(parent_class), -1);
	if (SQ_FAILED(sq_get(this->vm, -3))) {
		DEBUG(misc, 0, "[squirrel] Failed to initialize class '%s' based on parent class '%s'", class_name, parent_class);
		DEBUG(misc, 0, "[squirrel] Make sure that '%s' exists before trying to define '%s'", parent_class, class_name);
		return;
	}
	sq_newclass(this->vm, SQTrue);
}

void Squirrel::AddClassEnd()
{
	sq_newslot(vm, -3, SQFalse);
	sq_pop(vm, 1);
}

bool Squirrel::MethodExists(HSQOBJECT instance, const char *method_name)
{
	assert(!this->crashed);
	int top = sq_gettop(this->vm);
	/* Go to the instance-root */
	sq_pushobject(this->vm, instance);
	/* Find the function-name inside the script */
	sq_pushstring(this->vm, OTTD2SQ(method_name), -1);
	if (SQ_FAILED(sq_get(this->vm, -2))) {
		sq_settop(this->vm, top);
		return false;
	}
	sq_settop(this->vm, top);
	return true;
}

bool Squirrel::Resume(int suspend)
{
	assert(!this->crashed);
	/* Did we use more operations than we should have in the
	 * previous tick? If so, subtract that from the current run. */
	if (this->overdrawn_ops > 0 && suspend > 0) {
		this->overdrawn_ops -= suspend;
		/* Do we need to wait even more? */
		if (this->overdrawn_ops >= 0) return true;

		/* We can now only run whatever is "left". */
		suspend = -this->overdrawn_ops;
	}

	this->crashed = !sq_resumecatch(this->vm, suspend);
	this->overdrawn_ops = -this->vm->_ops_till_suspend;
	return this->vm->_suspended != 0;
}

void Squirrel::ResumeError()
{
	assert(!this->crashed);
	sq_resumeerror(this->vm);
}

void Squirrel::CollectGarbage()
{
	sq_collectgarbage(this->vm);
}

bool Squirrel::CallMethod(HSQOBJECT instance, const char *method_name, HSQOBJECT *ret, int suspend)
{
	assert(!this->crashed);
	/* Store the stack-location for the return value. We need to
	 * restore this after saving or the stack will be corrupted
	 * if we're in the middle of a DoCommand. */
	SQInteger last_target = this->vm->_suspended_target;
	/* Store the current top */
	int top = sq_gettop(this->vm);
	/* Go to the instance-root */
	sq_pushobject(this->vm, instance);
	/* Find the function-name inside the script */
	sq_pushstring(this->vm, OTTD2SQ(method_name), -1);
	if (SQ_FAILED(sq_get(this->vm, -2))) {
		DEBUG(misc, 0, "[squirrel] Could not find '%s' in the class", method_name);
		sq_settop(this->vm, top);
		return false;
	}
	/* Call the method */
	sq_pushobject(this->vm, instance);
	if (SQ_FAILED(sq_call(this->vm, 1, ret == NULL ? SQFalse : SQTrue, SQTrue, suspend))) return false;
	if (ret != NULL) sq_getstackobj(vm, -1, ret);
	/* Reset the top, but don't do so for the AI main function, as we need
	 *  a correct stack when resuming. */
	if (suspend == -1 || !this->IsSuspended()) sq_settop(this->vm, top);
	/* Restore the return-value location. */
	this->vm->_suspended_target = last_target;

	return true;
}

bool Squirrel::CallStringMethodStrdup(HSQOBJECT instance, const char *method_name, const char **res, int suspend)
{
	HSQOBJECT ret;
	if (!this->CallMethod(instance, method_name, &ret, suspend)) return false;
	if (ret._type != OT_STRING) return false;
	*res = strdup(ObjectToString(&ret));
	return true;
}

bool Squirrel::CallIntegerMethod(HSQOBJECT instance, const char *method_name, int *res, int suspend)
{
	HSQOBJECT ret;
	if (!this->CallMethod(instance, method_name, &ret, suspend)) return false;
	if (ret._type != OT_INTEGER) return false;
	*res = ObjectToInteger(&ret);
	return true;
}

bool Squirrel::CallBoolMethod(HSQOBJECT instance, const char *method_name, bool *res, int suspend)
{
	HSQOBJECT ret;
	if (!this->CallMethod(instance, method_name, &ret, suspend)) return false;
	if (ret._type != OT_BOOL) return false;
	*res = ObjectToBool(&ret);
	return true;
}

/* static */ bool Squirrel::CreateClassInstanceVM(HSQUIRRELVM vm, const char *class_name, void *real_instance, HSQOBJECT *instance, SQRELEASEHOOK release_hook)
{
	int oldtop = sq_gettop(vm);

	/* First, find the class */
	sq_pushroottable(vm);
	sq_pushstring(vm, OTTD2SQ(class_name), -1);
	if (SQ_FAILED(sq_get(vm, -2))) {
		DEBUG(misc, 0, "[squirrel] Failed to find class by the name '%s'", class_name);
		sq_settop(vm, oldtop);
		return false;
	}

	/* Create the instance */
	if (SQ_FAILED(sq_createinstance(vm, -1))) {
		DEBUG(misc, 0, "[squirrel] Failed to create instance for class '%s'", class_name);
		sq_settop(vm, oldtop);
		return false;
	}

	if (instance != NULL) {
		/* Find our instance */
		sq_getstackobj(vm, -1, instance);
		/* Add a reference to it, so it survives for ever */
		sq_addref(vm, instance);
	}
	sq_remove(vm, -2); // Class-name
	sq_remove(vm, -2); // Root-table

	/* Store it in the class */
	sq_setinstanceup(vm, -1, real_instance);
	if (release_hook != NULL) sq_setreleasehook(vm, -1, release_hook);

	if (instance != NULL) sq_settop(vm, oldtop);

	return true;
}

bool Squirrel::CreateClassInstance(const char *class_name, void *real_instance, HSQOBJECT *instance)
{
	return Squirrel::CreateClassInstanceVM(this->vm, class_name, real_instance, instance, NULL);
}

Squirrel::Squirrel()
{
	this->vm = sq_open(1024);
	this->print_func = NULL;
	this->global_pointer = NULL;
	this->crashed = false;
	this->overdrawn_ops = 0;

	/* Handle compile-errors ourself, so we can display it nicely */
	sq_setcompilererrorhandler(this->vm, &Squirrel::CompileError);
	sq_notifyallexceptions(this->vm, SQTrue);
	/* Set a good print-function */
	sq_setprintfunc(this->vm, &Squirrel::PrintFunc);
	/* Handle runtime-errors ourself, so we can display it nicely */
	sq_newclosure(this->vm, &Squirrel::_RunError, 0);
	sq_seterrorhandler(this->vm);

	/* Set the foreigh pointer, so we can always find this instance from within the VM */
	sq_setforeignptr(this->vm, this);

	sq_pushroottable(this->vm);
	squirrel_register_global_std(this);
}

class SQFile {
private:
	FILE *file;
	size_t size;
	size_t pos;

public:
	SQFile(FILE *file, size_t size) : file(file), size(size), pos(0) {}

	size_t Read(void *buf, size_t elemsize, size_t count)
	{
		assert(elemsize != 0);
		if (this->pos + (elemsize * count) > this->size) {
			count = (this->size - this->pos) / elemsize;
		}
		if (count == 0) return 0;
		size_t ret = fread(buf, elemsize, count, this->file);
		this->pos += ret * elemsize;
		return ret;
	}
};

static SQInteger _io_file_lexfeed_ASCII(SQUserPointer file)
{
	char c;
	if (((SQFile *)file)->Read(&c, sizeof(c), 1) > 0) return c;
	return 0;
}

static SQInteger _io_file_lexfeed_UTF8(SQUserPointer file)
{
	static const SQInteger utf8_lengths[16] =
	{
		1, 1, 1, 1, 1, 1, 1, 1, /* 0000 to 0111 : 1 byte (plain ASCII) */
		0, 0, 0, 0,             /* 1000 to 1011 : not valid */
		2, 2,                   /* 1100, 1101 : 2 bytes */
		3,                      /* 1110 : 3 bytes */
		4                       /* 1111 : 4 bytes */
	};
	static unsigned char byte_masks[5] = {0, 0, 0x1F, 0x0F, 0x07};
	unsigned char inchar;
	SQInteger c = 0;
	if (((SQFile *)file)->Read(&inchar, sizeof(inchar), 1) != 1) return 0;
	c = inchar;

	if (c >= 0x80) {
		SQInteger tmp;
		SQInteger codelen = utf8_lengths[c >> 4];
		if (codelen == 0) return 0;

		tmp = c & byte_masks[codelen];
		for (SQInteger n = 0; n < codelen - 1; n++) {
			tmp <<= 6;
			if (((SQFile *)file)->Read(&inchar, sizeof(inchar), 1) != 1) return 0;
			tmp |= inchar & 0x3F;
		}
		c = tmp;
	}
	return c;
}

static SQInteger _io_file_lexfeed_UCS2_LE(SQUserPointer file)
{
	wchar_t c;
	if (((SQFile *)file)->Read(&c, sizeof(c), 1) > 0) return (SQChar)c;
	return 0;
}

static SQInteger _io_file_lexfeed_UCS2_BE(SQUserPointer file)
{
	unsigned short c;
	if (((SQFile *)file)->Read(&c, sizeof(c), 1) > 0) {
		c = ((c >> 8) & 0x00FF)| ((c << 8) & 0xFF00);
		return (SQChar)c;
	}
	return 0;
}

static SQInteger _io_file_read(SQUserPointer file, SQUserPointer buf, SQInteger size)
{
	SQInteger ret = ((SQFile *)file)->Read(buf, 1, size);
	if (ret == 0) return -1;
	return ret;
}

/* static */ SQRESULT Squirrel::LoadFile(HSQUIRRELVM vm, const char *filename, SQBool printerror)
{
	size_t size;
	FILE *file = FioFOpenFile(filename, "rb", AI_DIR, &size);
	SQInteger ret;
	unsigned short us;
	unsigned char uc;
	SQLEXREADFUNC func;

	if (file != NULL) {
		SQFile f(file, size);
		ret = fread(&us, 1, sizeof(us), file);
		/* Most likely an empty file */
		if (ret != 2) us = 0;

		switch (us) {
			case SQ_BYTECODE_STREAM_TAG: { // BYTECODE
				fseek(file, -2, SEEK_CUR);
				if (SQ_SUCCEEDED(sq_readclosure(vm, _io_file_read, &f))) {
					FioFCloseFile(file);
					return SQ_OK;
				}
				FioFCloseFile(file);
				return sq_throwerror(vm, _SC("Couldn't read bytecode"));
			}
			case 0xFFFE: func = _io_file_lexfeed_UCS2_BE; break; // UTF-16 little endian
			case 0xFEFF: func = _io_file_lexfeed_UCS2_LE; break; // UTF-16 big endian
			case 0xBBEF: // UTF-8
				if (fread(&uc, 1, sizeof(uc), file) == 0) {
					FioFCloseFile(file);
					return sq_throwerror(vm, _SC("I/O error"));
				}
				if (uc != 0xBF) {
					FioFCloseFile(file);
					return sq_throwerror(vm, _SC("Unrecognized encoding"));
				}
				func = _io_file_lexfeed_UTF8;
				break;
			default: func = _io_file_lexfeed_ASCII; fseek(file, -2, SEEK_CUR); break; // ASCII
		}

		if (SQ_SUCCEEDED(sq_compile(vm, func, &f, OTTD2SQ(filename), printerror))) {
			FioFCloseFile(file);
			return SQ_OK;
		}
		FioFCloseFile(file);
		return SQ_ERROR;
	}
	return sq_throwerror(vm, _SC("cannot open the file"));
}

/* static */ bool Squirrel::LoadScript(HSQUIRRELVM vm, const char *script, bool in_root)
{
	/* Make sure we are always in the root-table */
	if (in_root) sq_pushroottable(vm);

	/* Load and run the script */
	if (SQ_SUCCEEDED(LoadFile(vm, script, SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue, 100000))) {
			sq_pop(vm, 1);
			return true;
		}
	}

	DEBUG(misc, 0, "[squirrel] Failed to compile '%s'", script);
	return false;
}

bool Squirrel::LoadScript(const char *script)
{
	return LoadScript(this->vm, script);
}

Squirrel::~Squirrel()
{
	/* Clean up the stuff */
	sq_pop(this->vm, 1);
	sq_close(this->vm);
}

void Squirrel::InsertResult(bool result)
{
	sq_pushbool(this->vm, result);
	vm->GetAt(vm->_stackbase + vm->_suspended_target) = vm->GetUp(-1);
	vm->Pop();
}

void Squirrel::InsertResult(int result)
{
	sq_pushinteger(this->vm, result);
	vm->GetAt(vm->_stackbase + vm->_suspended_target) = vm->GetUp(-1);
	vm->Pop();
}

/* static */ void Squirrel::DecreaseOps(HSQUIRRELVM vm, int ops)
{
	vm->DecreaseOps(ops);
}

bool Squirrel::IsSuspended()
{
	return this->vm->_suspended != 0;
}

bool Squirrel::HasScriptCrashed()
{
	return this->crashed;
}

void Squirrel::ResetCrashed()
{
	this->crashed = false;
}

void Squirrel::CrashOccurred()
{
	this->crashed = true;
}

bool Squirrel::CanSuspend()
{
	return sq_can_suspend(this->vm);
}
