/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file squirrel.cpp the implementation of the Squirrel class. It handles all Squirrel-stuff and gives a nice API back to work with. */

#include "../stdafx.h"
#include "../debug.h"
#include "squirrel_std.hpp"
#include "../fileio_func.h"
#include "../string_func.h"
#include "script_fatalerror.hpp"
#include "../settings_type.h"
#include <sqstdaux.h>
#include <../squirrel/sqpcheader.h>
#include <../squirrel/sqvm.h>
#include "../core/alloc_func.hpp"

/**
 * In the memory allocator for Squirrel we want to directly use malloc/realloc, so when the OS
 * does not have enough memory the game does not go into unrecoverable error mode and kill the
 * whole game, but rather let the AI die though then we need to circumvent MallocT/ReallocT.
 *
 * So no #include "../safeguards.h" here as is required, but after the allocator's implementation.
 */

/*
 * If changing the call paths into the scripting engine, define this symbol to enable full debugging of allocations.
 * This lets you track whether the allocator context is being switched correctly in all call paths.
#define SCRIPT_DEBUG_ALLOCATIONS
 */

struct ScriptAllocator {
	size_t allocated_size;   ///< Sum of allocated data size
	size_t allocation_limit; ///< Maximum this allocator may use before allocations fail
	/**
	 * Whether the error has already been thrown, so to not throw secondary errors in
	 * the handling of the allocation error. This as the handling of the error will
	 * throw a Squirrel error so the Squirrel stack can be dumped, however that gets
	 * allocated by this allocator and then you might end up in an infinite loop.
	 */
	bool error_thrown;

	static const size_t SAFE_LIMIT = 0x8000000; ///< 128 MiB, a safe choice for almost any situation

#ifdef SCRIPT_DEBUG_ALLOCATIONS
	std::map<void *, size_t> allocations;
#endif

	void CheckLimit() const
	{
		if (this->allocated_size > this->allocation_limit) throw Script_FatalError("Maximum memory allocation exceeded");
	}

	/**
	 * Catch all validation for the allocation; did it allocate too much memory according
	 * to the allocation limit or did the allocation at the OS level maybe fail? In those
	 * error situations a Script_FatalError is thrown, but once that has been done further
	 * allocations are allowed to make it possible for Squirrel to throw the error and
	 * clean everything up.
	 * @param requested_size The requested size that was requested to be allocated.
	 * @param p              The pointer to the allocated object, or null if allocation failed.
	 */
	void CheckAllocation(size_t requested_size, void *p)
	{
		if (this->allocated_size + requested_size > this->allocation_limit && !this->error_thrown) {
			/* Do not allow allocating more than the allocation limit, except when an error is
			 * already as then the allocation is for throwing that error in Squirrel, the
			 * associated stack trace information and while cleaning up the AI. */
			this->error_thrown = true;
			std::string msg = fmt::format("Maximum memory allocation exceeded by {} bytes when allocating {} bytes",
				this->allocated_size + requested_size - this->allocation_limit, requested_size);
			/* Don't leak the rejected allocation. */
			free(p);
			throw Script_FatalError(msg);
		}

		if (p == nullptr) {
			/* The OS did not have enough memory to allocate the object, regardless of the
			 * limit imposed by OpenTTD on the amount of memory that may be allocated. */
			if (this->error_thrown) {
				/* The allocation is called in the error handling of a memory allocation
				 * failure, then not being able to allocate that small amount of memory
				 * means there is no other choice than to bug out completely. */
				MallocError(requested_size);
			}

			this->error_thrown = true;
			std::string msg = fmt::format("Out of memory. Cannot allocate {} bytes", requested_size);
			throw Script_FatalError(msg);
		}
	}

	void *Malloc(SQUnsignedInteger size)
	{
		void *p = malloc(size);

		this->CheckAllocation(size, p);

		this->allocated_size += size;

#ifdef SCRIPT_DEBUG_ALLOCATIONS
		assert(p != nullptr);
		assert(this->allocations.find(p) == this->allocations.end());
		this->allocations[p] = size;
#endif

		return p;
	}

	void *Realloc(void *p, SQUnsignedInteger oldsize, SQUnsignedInteger size)
	{
		if (p == nullptr) {
			return this->Malloc(size);
		}
		if (size == 0) {
			this->Free(p, oldsize);
			return nullptr;
		}

#ifdef SCRIPT_DEBUG_ALLOCATIONS
		assert(this->allocations[p] == oldsize);
		this->allocations.erase(p);
#endif
		/* Can't use realloc directly because memory limit check.
		 * If memory exception is thrown, the old pointer is expected
		 * to be valid for engine cleanup.
		 */
		void *new_p = malloc(size);

		this->CheckAllocation(size - oldsize, new_p);

		/* Memory limit test passed, we can copy data and free old pointer. */
		memcpy(new_p, p, std::min(oldsize, size));
		free(p);

		this->allocated_size -= oldsize;
		this->allocated_size += size;

#ifdef SCRIPT_DEBUG_ALLOCATIONS
		assert(new_p != nullptr);
		assert(this->allocations.find(p) == this->allocations.end());
		this->allocations[new_p] = size;
#endif

		return new_p;
	}

	void Free(void *p, SQUnsignedInteger size)
	{
		if (p == nullptr) return;
		free(p);
		this->allocated_size -= size;

#ifdef SCRIPT_DEBUG_ALLOCATIONS
		assert(this->allocations.at(p) == size);
		this->allocations.erase(p);
#endif
	}

	ScriptAllocator()
	{
		this->allocated_size = 0;
		this->allocation_limit = static_cast<size_t>(_settings_game.script.script_max_memory_megabytes) << 20;
		if (this->allocation_limit == 0) this->allocation_limit = SAFE_LIMIT; // in case the setting is somehow zero
		this->error_thrown = false;
	}

	~ScriptAllocator()
	{
#ifdef SCRIPT_DEBUG_ALLOCATIONS
		assert(this->allocations.empty());
#endif
	}
};

/**
 * In the memory allocator for Squirrel we want to directly use malloc/realloc, so when the OS
 * does not have enough memory the game does not go into unrecoverable error mode and kill the
 * whole game, but rather let the AI die though then we need to circumvent MallocT/ReallocT.
 * For the rest of this code, the safeguards should be in place though!
 */
#include "../safeguards.h"

ScriptAllocator *_squirrel_allocator = nullptr;

/* See 3rdparty/squirrel/squirrel/sqmem.cpp for the default allocator implementation, which this overrides */
#ifndef SQUIRREL_DEFAULT_ALLOCATOR
void *sq_vm_malloc(SQUnsignedInteger size) { return _squirrel_allocator->Malloc(size); }
void *sq_vm_realloc(void *p, SQUnsignedInteger oldsize, SQUnsignedInteger size) { return _squirrel_allocator->Realloc(p, oldsize, size); }
void sq_vm_free(void *p, SQUnsignedInteger size) { _squirrel_allocator->Free(p, size); }
#endif

size_t Squirrel::GetAllocatedMemory() const noexcept
{
	assert(this->allocator != nullptr);
	return this->allocator->allocated_size;
}


void Squirrel::CompileError(HSQUIRRELVM vm, const SQChar *desc, const SQChar *source, SQInteger line, SQInteger column)
{
	std::string msg = fmt::format("Error {}:{}/{}: {}", source, line, column, desc);

	/* Check if we have a custom print function */
	Squirrel *engine = (Squirrel *)sq_getforeignptr(vm);
	engine->crashed = true;
	SQPrintFunc *func = engine->print_func;
	if (func == nullptr) {
		Debug(misc, 0, "[Squirrel] Compile error: {}", msg);
	} else {
		(*func)(true, msg);
	}
}

void Squirrel::ErrorPrintFunc(HSQUIRRELVM vm, const std::string &s)
{
	/* Check if we have a custom print function */
	SQPrintFunc *func = ((Squirrel *)sq_getforeignptr(vm))->print_func;
	if (func == nullptr) {
		fmt::print(stderr, "{}", s);
	} else {
		(*func)(true, s);
	}
}

void Squirrel::RunError(HSQUIRRELVM vm, const SQChar *error)
{
	/* Set the print function to something that prints to stderr */
	SQPRINTFUNCTION pf = sq_getprintfunc(vm);
	sq_setprintfunc(vm, &Squirrel::ErrorPrintFunc);

	/* Check if we have a custom print function */
	std::string msg = fmt::format("Your script made an error: {}\n", error);
	Squirrel *engine = (Squirrel *)sq_getforeignptr(vm);
	SQPrintFunc *func = engine->print_func;
	if (func == nullptr) {
		fmt::print(stderr, "{}", msg);
	} else {
		(*func)(true, msg);
	}

	/* Print below the error the stack, so the users knows what is happening */
	sqstd_printcallstack(vm);
	/* Reset the old print function */
	sq_setprintfunc(vm, pf);
}

SQInteger Squirrel::_RunError(HSQUIRRELVM vm)
{
	const SQChar *sErr = nullptr;

	if (sq_gettop(vm) >= 1) {
		if (SQ_SUCCEEDED(sq_getstring(vm, -1, &sErr))) {
			Squirrel::RunError(vm, sErr);
			return 0;
		}
	}

	Squirrel::RunError(vm, "unknown error");
	return 0;
}

void Squirrel::PrintFunc(HSQUIRRELVM vm, const std::string &s)
{
	/* Check if we have a custom print function */
	SQPrintFunc *func = ((Squirrel *)sq_getforeignptr(vm))->print_func;
	if (func == nullptr) {
		fmt::print("{}", s);
	} else {
		(*func)(false, s);
	}
}

void Squirrel::AddMethod(const char *method_name, SQFUNCTION proc, uint nparam, const char *params, void *userdata, int size)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushstring(this->vm, method_name, -1);

	if (size != 0) {
		void *ptr = sq_newuserdata(vm, size);
		memcpy(ptr, userdata, size);
	}

	sq_newclosure(this->vm, proc, size != 0 ? 1 : 0);
	if (nparam != 0) sq_setparamscheck(this->vm, nparam, params);
	sq_setnativeclosurename(this->vm, -1, method_name);
	sq_newslot(this->vm, -3, SQFalse);
}

void Squirrel::AddConst(const char *var_name, int value)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushstring(this->vm, var_name, -1);
	sq_pushinteger(this->vm, value);
	sq_newslot(this->vm, -3, SQTrue);
}

void Squirrel::AddConst(const char *var_name, bool value)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushstring(this->vm, var_name, -1);
	sq_pushbool(this->vm, value);
	sq_newslot(this->vm, -3, SQTrue);
}

void Squirrel::AddClassBegin(const char *class_name)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushroottable(this->vm);
	sq_pushstring(this->vm, class_name, -1);
	sq_newclass(this->vm, SQFalse);
}

void Squirrel::AddClassBegin(const char *class_name, const char *parent_class)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushroottable(this->vm);
	sq_pushstring(this->vm, class_name, -1);
	sq_pushstring(this->vm, parent_class, -1);
	if (SQ_FAILED(sq_get(this->vm, -3))) {
		Debug(misc, 0, "[squirrel] Failed to initialize class '{}' based on parent class '{}'", class_name, parent_class);
		Debug(misc, 0, "[squirrel] Make sure that '{}' exists before trying to define '{}'", parent_class, class_name);
		return;
	}
	sq_newclass(this->vm, SQTrue);
}

void Squirrel::AddClassEnd()
{
	ScriptAllocatorScope alloc_scope(this);

	sq_newslot(vm, -3, SQFalse);
	sq_pop(vm, 1);
}

bool Squirrel::MethodExists(HSQOBJECT instance, const char *method_name)
{
	assert(!this->crashed);
	ScriptAllocatorScope alloc_scope(this);

	int top = sq_gettop(this->vm);
	/* Go to the instance-root */
	sq_pushobject(this->vm, instance);
	/* Find the function-name inside the script */
	sq_pushstring(this->vm, method_name, -1);
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
	ScriptAllocatorScope alloc_scope(this);

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
	this->allocator->CheckLimit();
	return this->vm->_suspended != 0;
}

void Squirrel::ResumeError()
{
	assert(!this->crashed);
	ScriptAllocatorScope alloc_scope(this);
	sq_resumeerror(this->vm);
}

void Squirrel::CollectGarbage()
{
	ScriptAllocatorScope alloc_scope(this);
	sq_collectgarbage(this->vm);
}

bool Squirrel::CallMethod(HSQOBJECT instance, const char *method_name, HSQOBJECT *ret, int suspend)
{
	assert(!this->crashed);
	ScriptAllocatorScope alloc_scope(this);
	this->allocator->CheckLimit();

	/* Store the stack-location for the return value. We need to
	 * restore this after saving or the stack will be corrupted
	 * if we're in the middle of a DoCommand. */
	SQInteger last_target = this->vm->_suspended_target;
	/* Store the current top */
	int top = sq_gettop(this->vm);
	/* Go to the instance-root */
	sq_pushobject(this->vm, instance);
	/* Find the function-name inside the script */
	sq_pushstring(this->vm, method_name, -1);
	if (SQ_FAILED(sq_get(this->vm, -2))) {
		Debug(misc, 0, "[squirrel] Could not find '{}' in the class", method_name);
		sq_settop(this->vm, top);
		return false;
	}
	/* Call the method */
	sq_pushobject(this->vm, instance);
	if (SQ_FAILED(sq_call(this->vm, 1, ret == nullptr ? SQFalse : SQTrue, SQTrue, suspend))) return false;
	if (ret != nullptr) sq_getstackobj(vm, -1, ret);
	/* Reset the top, but don't do so for the script main function, as we need
	 *  a correct stack when resuming. */
	if (suspend == -1 || !this->IsSuspended()) sq_settop(this->vm, top);
	/* Restore the return-value location. */
	this->vm->_suspended_target = last_target;

	return true;
}

bool Squirrel::CallStringMethod(HSQOBJECT instance, const char *method_name, std::string *res, int suspend)
{
	HSQOBJECT ret;
	if (!this->CallMethod(instance, method_name, &ret, suspend)) return false;
	if (ret._type != OT_STRING) return false;
	*res = StrMakeValid(ObjectToString(&ret));
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

/* static */ bool Squirrel::CreateClassInstanceVM(HSQUIRRELVM vm, const std::string &class_name, void *real_instance, HSQOBJECT *instance, SQRELEASEHOOK release_hook, bool prepend_API_name)
{
	Squirrel *engine = (Squirrel *)sq_getforeignptr(vm);

	int oldtop = sq_gettop(vm);

	/* First, find the class */
	sq_pushroottable(vm);

	if (prepend_API_name) {
		std::string prepended_class_name = engine->GetAPIName();
		prepended_class_name += class_name;
		sq_pushstring(vm, prepended_class_name, -1);
	} else {
		sq_pushstring(vm, class_name, -1);
	}

	if (SQ_FAILED(sq_get(vm, -2))) {
		Debug(misc, 0, "[squirrel] Failed to find class by the name '{}{}'", prepend_API_name ? engine->GetAPIName() : "", class_name);
		sq_settop(vm, oldtop);
		return false;
	}

	/* Create the instance */
	if (SQ_FAILED(sq_createinstance(vm, -1))) {
		Debug(misc, 0, "[squirrel] Failed to create instance for class '{}{}'", prepend_API_name ? engine->GetAPIName() : "", class_name);
		sq_settop(vm, oldtop);
		return false;
	}

	if (instance != nullptr) {
		/* Find our instance */
		sq_getstackobj(vm, -1, instance);
		/* Add a reference to it, so it survives for ever */
		sq_addref(vm, instance);
	}
	sq_remove(vm, -2); // Class-name
	sq_remove(vm, -2); // Root-table

	/* Store it in the class */
	sq_setinstanceup(vm, -1, real_instance);
	if (release_hook != nullptr) sq_setreleasehook(vm, -1, release_hook);

	if (instance != nullptr) sq_settop(vm, oldtop);

	return true;
}

bool Squirrel::CreateClassInstance(const std::string &class_name, void *real_instance, HSQOBJECT *instance)
{
	ScriptAllocatorScope alloc_scope(this);
	return Squirrel::CreateClassInstanceVM(this->vm, class_name, real_instance, instance, nullptr);
}

Squirrel::Squirrel(const char *APIName) :
	APIName(APIName), allocator(new ScriptAllocator())
{
	this->Initialize();
}

void Squirrel::Initialize()
{
	ScriptAllocatorScope alloc_scope(this);

	this->global_pointer = nullptr;
	this->print_func = nullptr;
	this->crashed = false;
	this->overdrawn_ops = 0;
	this->vm = sq_open(1024);

	/* Handle compile-errors ourself, so we can display it nicely */
	sq_setcompilererrorhandler(this->vm, &Squirrel::CompileError);
	sq_notifyallexceptions(this->vm, _debug_script_level > 5);
	/* Set a good print-function */
	sq_setprintfunc(this->vm, &Squirrel::PrintFunc);
	/* Handle runtime-errors ourself, so we can display it nicely */
	sq_newclosure(this->vm, &Squirrel::_RunError, 0);
	sq_seterrorhandler(this->vm);

	/* Set the foreign pointer, so we can always find this instance from within the VM */
	sq_setforeignptr(this->vm, this);

	sq_pushroottable(this->vm);
	squirrel_register_global_std(this);

	/* Set consts table as delegate of root table, so consts/enums defined via require() are accessible */
	sq_pushconsttable(this->vm);
	sq_setdelegate(this->vm, -2);
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

static char32_t _io_file_lexfeed_ASCII(SQUserPointer file)
{
	unsigned char c;
	if (((SQFile *)file)->Read(&c, sizeof(c), 1) > 0) return c;
	return 0;
}

static char32_t _io_file_lexfeed_UTF8(SQUserPointer file)
{
	char buffer[5];

	/* Read the first character, and get the length based on UTF-8 specs. If invalid, bail out. */
	if (((SQFile *)file)->Read(buffer, sizeof(buffer[0]), 1) != 1) return 0;
	uint len = Utf8EncodedCharLen(buffer[0]);
	if (len == 0) return -1;

	/* Read the remaining bits. */
	if (len > 1 && ((SQFile *)file)->Read(buffer + 1, sizeof(buffer[0]), len - 1) != len - 1) return 0;

	/* Convert the character, and when definitely invalid, bail out as well. */
	char32_t c;
	if (Utf8Decode(&c, buffer) != len) return -1;

	return c;
}

static char32_t _io_file_lexfeed_UCS2_no_swap(SQUserPointer file)
{
	unsigned short c;
	if (((SQFile *)file)->Read(&c, sizeof(c), 1) > 0) return (char32_t)c;
	return 0;
}

static char32_t _io_file_lexfeed_UCS2_swap(SQUserPointer file)
{
	unsigned short c;
	if (((SQFile *)file)->Read(&c, sizeof(c), 1) > 0) {
		c = ((c >> 8) & 0x00FF)| ((c << 8) & 0xFF00);
		return (char32_t)c;
	}
	return 0;
}

static SQInteger _io_file_read(SQUserPointer file, SQUserPointer buf, SQInteger size)
{
	SQInteger ret = ((SQFile *)file)->Read(buf, 1, size);
	if (ret == 0) return -1;
	return ret;
}

SQRESULT Squirrel::LoadFile(HSQUIRRELVM vm, const std::string &filename, SQBool printerror)
{
	ScriptAllocatorScope alloc_scope(this);

	FILE *file;
	size_t size;
	if (strncmp(this->GetAPIName(), "AI", 2) == 0) {
		file = FioFOpenFile(filename, "rb", AI_DIR, &size);
		if (file == nullptr) file = FioFOpenFile(filename, "rb", AI_LIBRARY_DIR, &size);
	} else if (strncmp(this->GetAPIName(), "GS", 2) == 0) {
		file = FioFOpenFile(filename, "rb", GAME_DIR, &size);
		if (file == nullptr) file = FioFOpenFile(filename, "rb", GAME_LIBRARY_DIR, &size);
	} else {
		NOT_REACHED();
	}

	if (file == nullptr) {
		return sq_throwerror(vm, "cannot open the file");
	}
	unsigned short bom = 0;
	if (size >= 2) {
		[[maybe_unused]] size_t sr = fread(&bom, 1, sizeof(bom), file);
	}

	SQLEXREADFUNC func;
	switch (bom) {
		case SQ_BYTECODE_STREAM_TAG: { // BYTECODE
			if (fseek(file, -2, SEEK_CUR) < 0) {
				FioFCloseFile(file);
				return sq_throwerror(vm, "cannot seek the file");
			}

			SQFile f(file, size);
			if (SQ_SUCCEEDED(sq_readclosure(vm, _io_file_read, &f))) {
				FioFCloseFile(file);
				return SQ_OK;
			}
			FioFCloseFile(file);
			return sq_throwerror(vm, "Couldn't read bytecode");
		}
		case 0xFFFE:
			/* Either this file is encoded as big-endian and we're on a little-endian
			 * machine, or this file is encoded as little-endian and we're on a big-endian
			 * machine. Either way, swap the bytes of every word we read. */
			func = _io_file_lexfeed_UCS2_swap;
			size -= 2; // Skip BOM
			break;
		case 0xFEFF:
			func = _io_file_lexfeed_UCS2_no_swap;
			size -= 2; // Skip BOM
			break;
		case 0xBBEF:   // UTF-8
		case 0xEFBB: { // UTF-8 on big-endian machine
			/* Similarly, check the file is actually big enough to finish checking BOM */
			if (size < 3) {
				FioFCloseFile(file);
				return sq_throwerror(vm, "I/O error");
			}
			unsigned char uc;
			if (fread(&uc, 1, sizeof(uc), file) != sizeof(uc) || uc != 0xBF) {
				FioFCloseFile(file);
				return sq_throwerror(vm, "Unrecognized encoding");
			}
			func = _io_file_lexfeed_UTF8;
			size -= 3; // Skip BOM
			break;
		}
		default: // ASCII
			func = _io_file_lexfeed_ASCII;
			/* Account for when we might not have fread'd earlier */
			if (size >= 2 && fseek(file, -2, SEEK_CUR) < 0) {
				FioFCloseFile(file);
				return sq_throwerror(vm, "cannot seek the file");
			}
			break;
	}

	SQFile f(file, size);
	if (SQ_SUCCEEDED(sq_compile(vm, func, &f, filename.c_str(), printerror))) {
		FioFCloseFile(file);
		return SQ_OK;
	}
	FioFCloseFile(file);
	return SQ_ERROR;
}

bool Squirrel::LoadScript(HSQUIRRELVM vm, const std::string &script, bool in_root)
{
	ScriptAllocatorScope alloc_scope(this);

	/* Make sure we are always in the root-table */
	if (in_root) sq_pushroottable(vm);

	SQInteger ops_left = vm->_ops_till_suspend;
	/* Load and run the script */
	if (SQ_SUCCEEDED(LoadFile(vm, script, SQTrue))) {
		sq_push(vm, -2);
		if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue, 100000))) {
			sq_pop(vm, 1);
			/* After compiling the file we want to reset the amount of opcodes. */
			vm->_ops_till_suspend = ops_left;
			return true;
		}
	}

	vm->_ops_till_suspend = ops_left;
	Debug(misc, 0, "[squirrel] Failed to compile '{}'", script);
	return false;
}

bool Squirrel::LoadScript(const std::string &script)
{
	return LoadScript(this->vm, script);
}

Squirrel::~Squirrel()
{
	this->Uninitialize();
}

void Squirrel::Uninitialize()
{
	ScriptAllocatorScope alloc_scope(this);

	/* Remove the delegation */
	sq_pushroottable(this->vm);
	sq_pushnull(this->vm);
	sq_setdelegate(this->vm, -2);
	sq_pop(this->vm, 1);

	/* Clean up the stuff */
	sq_pop(this->vm, 1);
	sq_close(this->vm);

	assert(this->allocator->allocated_size == 0);

	/* Reset memory allocation errors. */
	this->allocator->error_thrown = false;
}

void Squirrel::Reset()
{
	this->Uninitialize();
	this->Initialize();
}

void Squirrel::InsertResult(bool result)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushbool(this->vm, result);
	if (this->IsSuspended()) { // Called before resuming a suspended script?
		vm->GetAt(vm->_stackbase + vm->_suspended_target) = vm->GetUp(-1);
		vm->Pop();
	}
}

void Squirrel::InsertResult(int result)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushinteger(this->vm, result);
	if (this->IsSuspended()) { // Called before resuming a suspended script?
		vm->GetAt(vm->_stackbase + vm->_suspended_target) = vm->GetUp(-1);
		vm->Pop();
	}
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

void Squirrel::CrashOccurred()
{
	this->crashed = true;
}

bool Squirrel::CanSuspend()
{
	ScriptAllocatorScope alloc_scope(this);
	return sq_can_suspend(this->vm);
}

SQInteger Squirrel::GetOpsTillSuspend()
{
	return this->vm->_ops_till_suspend;
}
