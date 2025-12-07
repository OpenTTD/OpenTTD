/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file squirrel.cpp the implementation of the Squirrel class. It handles all Squirrel-stuff and gives a nice API back to work with. */

#include "../stdafx.h"
#include "../debug.h"
#include "squirrel_std.hpp"
#include "../error_func.h"
#include "../fileio_func.h"
#include "../string_func.h"
#include "script_fatalerror.hpp"
#include "../settings_type.h"
#include <sqstdaux.h>
#include <../squirrel/sqpcheader.h>
#include <../squirrel/sqvm.h>
#include "../core/math_func.hpp"
#include "../core/string_consumer.hpp"

#include "../safeguards.h"

/*
 * If changing the call paths into the scripting engine, define this symbol to enable full debugging of allocations.
 * This lets you track whether the allocator context is being switched correctly in all call paths.
#define SCRIPT_DEBUG_ALLOCATIONS
 */

struct ScriptAllocator {
private:
	std::allocator<uint8_t> allocator;
	size_t allocated_size = 0; ///< Sum of allocated data size
	size_t allocation_limit; ///< Maximum this allocator may use before allocations fail
	/**
	 * Whether the error has already been thrown, so to not throw secondary errors in
	 * the handling of the allocation error. This as the handling of the error will
	 * throw a Squirrel error so the Squirrel stack can be dumped, however that gets
	 * allocated by this allocator and then you might end up in an infinite loop.
	 */
	bool error_thrown = false;

	static const size_t SAFE_LIMIT = 0x8000000; ///< 128 MiB, a safe choice for almost any situation

#ifdef SCRIPT_DEBUG_ALLOCATIONS
	std::map<void *, size_t> allocations;
#endif

	/**
	 * Checks whether an allocation is allowed by the memory limit set for the script.
	 * @param requested_size The requested size that was requested to be allocated.
	 * @throws Script_FatalError When memory may not be allocated (limit reached, except for error handling).
	 */
	void CheckAllocationAllowed(size_t requested_size)
	{
		/* When an error has been thrown, we are allocating just a bit of memory for the stack trace. */
		if (this->error_thrown) return;

		if (this->allocated_size + requested_size <= this->allocation_limit) return;

		/* Do not allow allocating more than the allocation limit. */
		this->error_thrown = true;
		std::string msg = fmt::format("Maximum memory allocation exceeded by {} bytes when allocating {} bytes",
			this->allocated_size + requested_size - this->allocation_limit, requested_size);
		throw Script_FatalError(msg);
	}

	/**
	 * Internal helper to allocate the given amount of bytes.
	 * @param requested_size The requested size.
	 * @return The allocated memory.
	 * @throws Script_FatalError When memory could not be allocated.
	 */
	void *DoAlloc(SQUnsignedInteger requested_size)
	{
		try {
			void *p = this->allocator.allocate(requested_size);
			assert(p != nullptr);
			this->allocated_size += requested_size;

#ifdef SCRIPT_DEBUG_ALLOCATIONS
			assert(this->allocations.find(p) == this->allocations.end());
			this->allocations[p] = requested_size;
#endif
			return p;
		} catch (const std::bad_alloc &) {
			/* The OS did not have enough memory to allocate the object, regardless of the
			 * limit imposed by OpenTTD on the amount of memory that may be allocated. */
			if (this->error_thrown) {
				/* The allocation is called in the error handling of a memory allocation
				 * failure, then not being able to allocate that small amount of memory
				 * means there is no other choice than to bug out completely. */
				FatalError("Out of memory. Cannot allocate {} bytes", requested_size);
			}

			this->error_thrown = true;
			std::string msg = fmt::format("Out of memory. Cannot allocate {} bytes", requested_size);
			throw Script_FatalError(msg);
		}
	}

public:
	size_t GetAllocatedSize() const { return this->allocated_size; }

	void CheckLimit() const
	{
		if (this->allocated_size > this->allocation_limit) throw Script_FatalError("Maximum memory allocation exceeded");
	}

	void Reset()
	{
		assert(this->allocated_size == 0);
		this->error_thrown = false;
	}

	void *Malloc(SQUnsignedInteger size)
	{
		this->CheckAllocationAllowed(size);
		return this->DoAlloc(size);
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

		this->CheckAllocationAllowed(size - oldsize);

		void *new_p = this->DoAlloc(size);
		std::copy_n(static_cast<std::byte *>(p), std::min(oldsize, size), static_cast<std::byte *>(new_p));
		this->Free(p, oldsize);

		return new_p;
	}

	void Free(void *p, SQUnsignedInteger size)
	{
		if (p == nullptr) return;
		this->allocator.deallocate(reinterpret_cast<uint8_t*>(p), size);
		this->allocated_size -= size;

#ifdef SCRIPT_DEBUG_ALLOCATIONS
		assert(this->allocations.at(p) == size);
		this->allocations.erase(p);
#endif
	}

	ScriptAllocator()
	{
		this->allocation_limit = static_cast<size_t>(_settings_game.script.script_max_memory_megabytes) << 20;
		if (this->allocation_limit == 0) this->allocation_limit = SAFE_LIMIT; // in case the setting is somehow zero
	}

	~ScriptAllocator()
	{
#ifdef SCRIPT_DEBUG_ALLOCATIONS
		assert(this->allocations.empty());
#endif
	}
};

ScriptAllocator *_squirrel_allocator = nullptr;

void *sq_vm_malloc(SQUnsignedInteger size) { return _squirrel_allocator->Malloc(size); }
void *sq_vm_realloc(void *p, SQUnsignedInteger oldsize, SQUnsignedInteger size) { return _squirrel_allocator->Realloc(p, oldsize, size); }
void sq_vm_free(void *p, SQUnsignedInteger size) { _squirrel_allocator->Free(p, size); }

size_t Squirrel::GetAllocatedMemory() const noexcept
{
	assert(this->allocator != nullptr);
	return this->allocator->GetAllocatedSize();
}


void Squirrel::CompileError(HSQUIRRELVM vm, std::string_view desc, std::string_view source, SQInteger line, SQInteger column)
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

void Squirrel::ErrorPrintFunc(HSQUIRRELVM vm, std::string_view s)
{
	/* Check if we have a custom print function */
	SQPrintFunc *func = ((Squirrel *)sq_getforeignptr(vm))->print_func;
	if (func == nullptr) {
		fmt::print(stderr, "{}", s);
	} else {
		(*func)(true, s);
	}
}

void Squirrel::RunError(HSQUIRRELVM vm, std::string_view error)
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
	std::string_view view;

	if (sq_gettop(vm) >= 1) {
		if (SQ_SUCCEEDED(sq_getstring(vm, -1, view))) {
			Squirrel::RunError(vm, view);
			return 0;
		}
	}

	Squirrel::RunError(vm, "unknown error");
	return 0;
}

void Squirrel::PrintFunc(HSQUIRRELVM vm, std::string_view s)
{
	/* Check if we have a custom print function */
	SQPrintFunc *func = ((Squirrel *)sq_getforeignptr(vm))->print_func;
	if (func == nullptr) {
		fmt::print("{}", s);
	} else {
		(*func)(false, s);
	}
}

void Squirrel::AddMethod(std::string_view method_name, SQFUNCTION proc, std::string_view params, void *userdata, int size)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushstring(this->vm, method_name);

	if (size != 0) {
		void *ptr = sq_newuserdata(vm, size);
		std::copy_n(static_cast<std::byte *>(userdata), size, static_cast<std::byte *>(ptr));
	}

	sq_newclosure(this->vm, proc, size != 0 ? 1 : 0);
	if (!params.empty()) sq_setparamscheck(this->vm, params.size(), params);
	sq_setnativeclosurename(this->vm, -1, method_name);
	sq_newslot(this->vm, -3, SQFalse);
}

void Squirrel::AddConst(std::string_view var_name, SQInteger value)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushstring(this->vm, var_name);
	sq_pushinteger(this->vm, value);
	sq_newslot(this->vm, -3, SQTrue);
}

void Squirrel::AddConst(std::string_view var_name, bool value)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushstring(this->vm, var_name);
	sq_pushbool(this->vm, value);
	sq_newslot(this->vm, -3, SQTrue);
}

void Squirrel::AddClassBegin(std::string_view class_name)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushroottable(this->vm);
	sq_pushstring(this->vm, class_name);
	sq_newclass(this->vm, SQFalse);
}

void Squirrel::AddClassBegin(std::string_view class_name, std::string_view parent_class)
{
	ScriptAllocatorScope alloc_scope(this);

	sq_pushroottable(this->vm);
	sq_pushstring(this->vm, class_name);
	sq_pushstring(this->vm, parent_class);
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

bool Squirrel::MethodExists(HSQOBJECT instance, std::string_view method_name)
{
	assert(!this->crashed);
	ScriptAllocatorScope alloc_scope(this);

	int top = sq_gettop(this->vm);
	/* Go to the instance-root */
	sq_pushobject(this->vm, instance);
	/* Find the function-name inside the script */
	sq_pushstring(this->vm, method_name);
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

bool Squirrel::CallMethod(HSQOBJECT instance, std::string_view method_name, HSQOBJECT *ret, int suspend)
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
	sq_pushstring(this->vm, method_name);
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

bool Squirrel::CallStringMethod(HSQOBJECT instance, std::string_view method_name, std::string *res, int suspend)
{
	HSQOBJECT ret;
	if (!this->CallMethod(instance, method_name, &ret, suspend)) return false;

	auto str = ObjectToString(&ret);
	if (!str.has_value()) return false;

	*res = StrMakeValid(*str);
	return true;
}

bool Squirrel::CallIntegerMethod(HSQOBJECT instance, std::string_view method_name, int *res, int suspend)
{
	HSQOBJECT ret;
	if (!this->CallMethod(instance, method_name, &ret, suspend)) return false;
	if (ret._type != OT_INTEGER) return false;
	*res = ObjectToInteger(&ret);
	return true;
}

bool Squirrel::CallBoolMethod(HSQOBJECT instance, std::string_view method_name, bool *res, int suspend)
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
		std::string prepended_class_name = fmt::format("{}{}", engine->GetAPIName(), class_name);
		sq_pushstring(vm, prepended_class_name);
	} else {
		sq_pushstring(vm, class_name);
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

/* static */ SQUserPointer Squirrel::GetRealInstance(HSQUIRRELVM vm, int index, std::string_view tag)
{
	if (index < 0) index += sq_gettop(vm) + 1;
	Squirrel *engine = static_cast<Squirrel *>(sq_getforeignptr(vm));
	std::string class_name = fmt::format("{}{}", engine->GetAPIName(), tag);
	sq_pushroottable(vm);
	sq_pushstring(vm, class_name);
	sq_get(vm, -2);
	sq_push(vm, index);
	if (sq_instanceof(vm) == SQTrue) {
		sq_pop(vm, 3);
		SQUserPointer ptr = nullptr;
		if (SQ_SUCCEEDED(sq_getinstanceup(vm, index, &ptr, nullptr))) return ptr;
	}
	throw sq_throwerror(vm, fmt::format("parameter {} has an invalid type ; expected: '{}'", index - 1, class_name));
}

Squirrel::Squirrel(std::string_view api_name) :
	api_name(api_name), allocator(std::make_unique<ScriptAllocator>())
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
	squirrel_register_global_std(*this);

	/* Set consts table as delegate of root table, so consts/enums defined via require() are accessible */
	sq_pushconsttable(this->vm);
	sq_setdelegate(this->vm, -2);
}

class SQFile {
private:
	FileHandle file;
	size_t size;
	size_t pos;
	std::string buffer;
	StringConsumer consumer;

	size_t ReadInternal(std::span<char> buf)
	{
		size_t count = buf.size();
		if (this->pos + count > this->size) {
			count = this->size - this->pos;
		}
		if (count > 0) count = fread(buf.data(), 1, count, this->file);
		this->pos += count;
		return count;
	}

public:
	SQFile(FileHandle file, size_t size) : file(std::move(file)), size(size), pos(0), consumer(buffer) {}

	StringConsumer &GetConsumer(size_t min_size = 64)
	{
		if (this->consumer.GetBytesLeft() < min_size && this->pos < this->size) {
			this->buffer.erase(0, this->consumer.GetBytesRead());

			size_t buffer_size = this->buffer.size();
			size_t read_size = Align(min_size - buffer_size, 4096); // read pages of 4096 bytes
			/* TODO C++23: use std::string::resize_and_overwrite() */
			this->buffer.resize(buffer_size + read_size);
			auto dest = std::span(this->buffer.data(), this->buffer.size()).subspan(buffer_size);
			buffer_size += this->ReadInternal(dest);
			this->buffer.resize(buffer_size);

			this->consumer = StringConsumer(this->buffer);
		}
		return this->consumer;
	}

	size_t Read(void *buf, size_t max_size)
	{
		std::span<char> dest(reinterpret_cast<char *>(buf), max_size);

		auto view = this->consumer.Read(max_size);
		std::copy(view.data(), view.data() + view.size(), dest.data());
		size_t result_size = view.size();

		if (result_size < max_size) {
			assert(!this->consumer.AnyBytesLeft());
			result_size += this->ReadInternal(dest.subspan(result_size));
		}

		return result_size;
	}
};

static char32_t _io_file_lexfeed_ASCII(SQUserPointer file)
{
	StringConsumer &consumer = reinterpret_cast<SQFile *>(file)->GetConsumer();
	return consumer.TryReadUint8().value_or(0); // read as unsigned, otherwise integer promotion breaks it
}

static char32_t _io_file_lexfeed_UTF8(SQUserPointer file)
{
	StringConsumer &consumer = reinterpret_cast<SQFile *>(file)->GetConsumer();
	return consumer.AnyBytesLeft() ? consumer.ReadUtf8(-1) : 0;
}

static SQInteger _io_file_read(SQUserPointer file, SQUserPointer buf, SQInteger size)
{
	SQInteger ret = reinterpret_cast<SQFile *>(file)->Read(buf, size);
	if (ret == 0) return -1;
	return ret;
}

SQRESULT Squirrel::LoadFile(HSQUIRRELVM vm, const std::string &filename, SQBool printerror)
{
	ScriptAllocatorScope alloc_scope(this);

	std::optional<FileHandle> file = std::nullopt;
	size_t size;
	if (this->GetAPIName().starts_with("AI")) {
		file = FioFOpenFile(filename, "rb", AI_DIR, &size);
		if (!file.has_value()) file = FioFOpenFile(filename, "rb", AI_LIBRARY_DIR, &size);
	} else if (this->GetAPIName().starts_with("GS")) {
		file = FioFOpenFile(filename, "rb", GAME_DIR, &size);
		if (!file.has_value()) file = FioFOpenFile(filename, "rb", GAME_LIBRARY_DIR, &size);
	} else {
		NOT_REACHED();
	}

	if (!file.has_value()) {
		return sq_throwerror(vm, "cannot open the file");
	}
	unsigned short bom = 0;
	if (size >= 2) {
		if (fread(&bom, 1, sizeof(bom), *file) != sizeof(bom)) return sq_throwerror(vm, "cannot read the file");;
	}

	SQLEXREADFUNC func;
	switch (bom) {
		case SQ_BYTECODE_STREAM_TAG: { // BYTECODE
			if (fseek(*file, -2, SEEK_CUR) < 0) {
				return sq_throwerror(vm, "cannot seek the file");
			}

			SQFile f(std::move(*file), size);
			if (SQ_SUCCEEDED(sq_readclosure(vm, _io_file_read, &f))) {
				return SQ_OK;
			}
			return sq_throwerror(vm, "Couldn't read bytecode");
		}
		case 0xBBEF:   // UTF-8
		case 0xEFBB: { // UTF-8 on big-endian machine
			/* Similarly, check the file is actually big enough to finish checking BOM */
			if (size < 3) {
				return sq_throwerror(vm, "I/O error");
			}
			unsigned char uc;
			if (fread(&uc, 1, sizeof(uc), *file) != sizeof(uc) || uc != 0xBF) {
				return sq_throwerror(vm, "Unrecognized encoding");
			}
			func = _io_file_lexfeed_UTF8;
			size -= 3; // Skip BOM
			break;
		}
		default: // ASCII
			func = _io_file_lexfeed_ASCII;
			/* Account for when we might not have fread'd earlier */
			if (size >= 2 && fseek(*file, -2, SEEK_CUR) < 0) {
				return sq_throwerror(vm, "cannot seek the file");
			}
			break;
	}

	SQFile f(std::move(*file), size);
	if (SQ_SUCCEEDED(sq_compile(vm, func, &f, filename.c_str(), printerror))) {
		return SQ_OK;
	}
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

	/* Reset memory allocation errors. */
	this->allocator->Reset();
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
