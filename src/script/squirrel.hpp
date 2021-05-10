/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file squirrel.hpp defines the Squirrel class */

#ifndef SQUIRREL_HPP
#define SQUIRREL_HPP

#include <squirrel.h>

/** The type of script we're working with, i.e. for who is it? */
enum ScriptType {
	ST_AI, ///< The script is for AI scripts.
	ST_GS, ///< The script is for Game scripts.
};

struct ScriptAllocator;

class Squirrel {
	friend class ScriptAllocatorScope;

private:
	typedef void (SQPrintFunc)(bool error_msg, const SQChar *message);

	HSQUIRRELVM vm;          ///< The VirtualMachine instance for squirrel
	void *global_pointer;    ///< Can be set by who ever initializes Squirrel
	SQPrintFunc *print_func; ///< Points to either nullptr, or a custom print handler
	bool crashed;            ///< True if the squirrel script made an error.
	int overdrawn_ops;       ///< The amount of operations we have overdrawn.
	const char *APIName;     ///< Name of the API used for this squirrel.
	std::unique_ptr<ScriptAllocator> allocator; ///< Allocator object used by this script.

	/**
	 * The internal RunError handler. It looks up the real error and calls RunError with it.
	 */
	static SQInteger _RunError(HSQUIRRELVM vm);

	/**
	 * Get the API name.
	 */
	const char *GetAPIName() { return this->APIName; }

	/** Perform all initialization steps to create the engine. */
	void Initialize();
	/** Perform all the cleanups for the engine. */
	void Uninitialize();

protected:
	/**
	 * The CompileError handler.
	 */
	static void CompileError(HSQUIRRELVM vm, const SQChar *desc, const SQChar *source, SQInteger line, SQInteger column);

	/**
	 * The RunError handler.
	 */
	static void RunError(HSQUIRRELVM vm, const SQChar *error);

	/**
	 * If a user runs 'print' inside a script, this function gets the params.
	 */
	static void PrintFunc(HSQUIRRELVM vm, const SQChar *s, ...) WARN_FORMAT(2, 3);

	/**
	 * If an error has to be print, this function is called.
	 */
	static void ErrorPrintFunc(HSQUIRRELVM vm, const SQChar *s, ...) WARN_FORMAT(2, 3);

public:
	Squirrel(const char *APIName);
	~Squirrel();

	/**
	 * Get the squirrel VM. Try to avoid using this.
	 */
	HSQUIRRELVM GetVM() { return this->vm; }

	/**
	 * Load a script.
	 * @param script The full script-name to load.
	 * @return False if loading failed.
	 */
	bool LoadScript(const char *script);
	bool LoadScript(HSQUIRRELVM vm, const char *script, bool in_root = true);

	/**
	 * Load a file to a given VM.
	 */
	SQRESULT LoadFile(HSQUIRRELVM vm, const char *filename, SQBool printerror);

	/**
	 * Adds a function to the stack. Depending on the current state this means
	 *  either a method or a global function.
	 */
	void AddMethod(const char *method_name, SQFUNCTION proc, uint nparam = 0, const char *params = nullptr, void *userdata = nullptr, int size = 0);

	/**
	 * Adds a const to the stack. Depending on the current state this means
	 *  either a const to a class or to the global space.
	 */
	void AddConst(const char *var_name, int value);

	/**
	 * Adds a const to the stack. Depending on the current state this means
	 *  either a const to a class or to the global space.
	 */
	void AddConst(const char *var_name, uint value) { this->AddConst(var_name, (int)value); }

	/**
	 * Adds a const to the stack. Depending on the current state this means
	 *  either a const to a class or to the global space.
	 */
	void AddConst(const char *var_name, bool value);

	/**
	 * Adds a class to the global scope. Make sure to call AddClassEnd when you
	 *  are done adding methods.
	 */
	void AddClassBegin(const char *class_name);

	/**
	 * Adds a class to the global scope, extending 'parent_class'.
	 * Make sure to call AddClassEnd when you are done adding methods.
	 */
	void AddClassBegin(const char *class_name, const char *parent_class);

	/**
	 * Finishes adding a class to the global scope. If this isn't called, no
	 *  class is really created.
	 */
	void AddClassEnd();

	/**
	 * Resume a VM when it was suspended via a throw.
	 */
	bool Resume(int suspend = -1);

	/**
	 * Resume the VM with an error so it prints a stack trace.
	 */
	void ResumeError();

	/**
	 * Tell the VM to do a garbage collection run.
	 */
	void CollectGarbage();

	void InsertResult(bool result);
	void InsertResult(int result);
	void InsertResult(uint result) { this->InsertResult((int)result); }

	/**
	 * Call a method of an instance, in various flavors.
	 * @return False if the script crashed or returned a wrong type.
	 */
	bool CallMethod(HSQOBJECT instance, const char *method_name, HSQOBJECT *ret, int suspend);
	bool CallMethod(HSQOBJECT instance, const char *method_name, int suspend) { return this->CallMethod(instance, method_name, nullptr, suspend); }
	bool CallStringMethodStrdup(HSQOBJECT instance, const char *method_name, const char **res, int suspend);
	bool CallIntegerMethod(HSQOBJECT instance, const char *method_name, int *res, int suspend);
	bool CallBoolMethod(HSQOBJECT instance, const char *method_name, bool *res, int suspend);

	/**
	 * Check if a method exists in an instance.
	 */
	bool MethodExists(HSQOBJECT instance, const char *method_name);

	/**
	 * Creates a class instance.
	 * @param vm The VM to create the class instance for
	 * @param class_name The name of the class of which we create an instance.
	 * @param real_instance The instance to the real class, if it represents a real class.
	 * @param instance Returning value with the pointer to the instance.
	 * @param release_hook Optional param to give a release hook.
	 * @param prepend_API_name Optional parameter; if true, the class_name is prefixed with the current API name.
	 * @return False if creating failed.
	 */
	static bool CreateClassInstanceVM(HSQUIRRELVM vm, const char *class_name, void *real_instance, HSQOBJECT *instance, SQRELEASEHOOK release_hook, bool prepend_API_name = false);

	/**
	 * Exactly the same as CreateClassInstanceVM, only callable without instance of Squirrel.
	 */
	bool CreateClassInstance(const char *class_name, void *real_instance, HSQOBJECT *instance);

	/**
	 * Get the real-instance pointer.
	 * @note This will only work just after a function-call from within Squirrel
	 *  to your C++ function.
	 */
	static bool GetRealInstance(HSQUIRRELVM vm, SQUserPointer *ptr) { return SQ_SUCCEEDED(sq_getinstanceup(vm, 1, ptr, 0)); }

	/**
	 * Get the Squirrel-instance pointer.
	 * @note This will only work just after a function-call from within Squirrel
	 *  to your C++ function.
	 */
	static bool GetInstance(HSQUIRRELVM vm, HSQOBJECT *ptr, int pos = 1) { sq_getclass(vm, pos); sq_getstackobj(vm, pos, ptr); sq_pop(vm, 1); return true; }

	/**
	 * Convert a Squirrel-object to a string.
	 */
	static const char *ObjectToString(HSQOBJECT *ptr) { return sq_objtostring(ptr); }

	/**
	 * Convert a Squirrel-object to an integer.
	 */
	static int ObjectToInteger(HSQOBJECT *ptr) { return sq_objtointeger(ptr); }

	/**
	 * Convert a Squirrel-object to a bool.
	 */
	static bool ObjectToBool(HSQOBJECT *ptr) { return sq_objtobool(ptr) == 1; }

	/**
	 * Sets a pointer in the VM that is reachable from where ever you are in SQ.
	 *  Useful to keep track of the main instance.
	 */
	void SetGlobalPointer(void *ptr) { this->global_pointer = ptr; }

	/**
	 * Get the pointer as set by SetGlobalPointer.
	 */
	static void *GetGlobalPointer(HSQUIRRELVM vm) { return ((Squirrel *)sq_getforeignptr(vm))->global_pointer; }

	/**
	 * Set a custom print function, so you can handle outputs from SQ yourself.
	 */
	void SetPrintFunction(SQPrintFunc *func) { this->print_func = func; }

	/**
	 * Throw a Squirrel error that will be nicely displayed to the user.
	 */
	void ThrowError(const char *error) { sq_throwerror(this->vm, error); }

	/**
	 * Release a SQ object.
	 */
	void ReleaseObject(HSQOBJECT *ptr) { sq_release(this->vm, ptr); }

	/**
	 * Tell the VM to remove \c amount ops from the number of ops till suspend.
	 */
	static void DecreaseOps(HSQUIRRELVM vm, int amount);

	/**
	 * Did the squirrel code suspend or return normally.
	 * @return True if the function suspended.
	 */
	bool IsSuspended();

	/**
	 * Find out if the squirrel script made an error before.
	 */
	bool HasScriptCrashed();

	/**
	 * Set the script status to crashed.
	 */
	void CrashOccurred();

	/**
	 * Are we allowed to suspend the squirrel script at this moment?
	 */
	bool CanSuspend();

	/**
	 * How many operations can we execute till suspension?
	 */
	SQInteger GetOpsTillSuspend();

	/**
	 * Completely reset the engine; start from scratch.
	 */
	void Reset();

	/**
	 * Get number of bytes allocated by this VM.
	 */
	size_t GetAllocatedMemory() const noexcept;
};


extern ScriptAllocator *_squirrel_allocator;

class ScriptAllocatorScope {
	ScriptAllocator *old_allocator;

public:
	ScriptAllocatorScope(const Squirrel *engine)
	{
		this->old_allocator = _squirrel_allocator;
		/* This may get called with a nullptr engine, in case of a crashed script */
		_squirrel_allocator = engine != nullptr ? engine->allocator.get() : nullptr;
	}

	~ScriptAllocatorScope()
	{
		_squirrel_allocator = this->old_allocator;
	}
};

#endif /* SQUIRREL_HPP */
