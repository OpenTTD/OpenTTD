/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file squirrel.hpp Defines the Squirrel class. */

#ifndef SQUIRREL_HPP
#define SQUIRREL_HPP

#include <squirrel.h>
#include "../core/convertible_through_base.hpp"

/** The type of script we're working with, i.e. for who is it? */
enum class ScriptType : uint8_t {
	AI, ///< The script is for AI scripts.
	GS, ///< The script is for Game scripts.
};

struct ScriptAllocator;

class Squirrel {
	friend class ScriptAllocatorScope;
	friend class ScriptInstance;

private:
	using SQPrintFunc = void (bool error_msg, std::string_view message);

	HSQUIRRELVM vm;          ///< The VirtualMachine instance for squirrel
	void *global_pointer;    ///< Can be set by who ever initializes Squirrel
	SQPrintFunc *print_func; ///< Points to either nullptr, or a custom print handler
	bool crashed;            ///< True if the squirrel script made an error.
	int overdrawn_ops;       ///< The amount of operations we have overdrawn.
	std::string_view api_name; ///< Name of the API used for this squirrel.
	std::unique_ptr<ScriptAllocator> allocator; ///< Allocator object used by this script.

	/**
	 * The internal RunError handler. It looks up the real error and calls RunError with it.
	 * @param vm The virtual machine.
	 * @return Always \c 0. Required return type due to this being passed to another function.
	 */
	static SQInteger _RunError(HSQUIRRELVM vm);

	/**
	 * Get the API name.
	 * @return The name of the API (AI/GS).
	 */
	std::string_view GetAPIName() { return this->api_name; }

	/** Perform all initialization steps to create the engine. */
	void Initialize();
	/** Perform all the cleanups for the engine. */
	void Uninitialize();

protected:
	/**
	 * The CompileError handler.
	 * @param vm The virtual machine the error occurred for.
	 * @param desc A description of the error.
	 * @param source The file the error occurred in.
	 * @param line The line the error was in.
	 * @param column The column the error was in.
	 */
	static void CompileError(HSQUIRRELVM vm, std::string_view desc, std::string_view source, SQInteger line, SQInteger column);

	/**
	 * The RunError handler.
	 * @param vm The virtual machine the error occurred for.
	 * @param error A description of the error.
	 */
	static void RunError(HSQUIRRELVM vm, std::string_view error);

	/**
	 * If a user runs 'print' inside a script, this function gets the params.
	 * @param vm The virtual machine to print for.
	 * @param s The text to print.
	 */
	static void PrintFunc(HSQUIRRELVM vm, std::string_view s);

	/**
	 * If an error has to be print, this function is called.
	 * @param vm The virtual machine to print for.
	 * @param s The eroror text to print.
	 */
	static void ErrorPrintFunc(HSQUIRRELVM vm, std::string_view s);

public:
	Squirrel(std::string_view api_name);
	~Squirrel();

	/**
	 * Get the squirrel VM. Try to avoid using this.
	 * @return Our virtual machine.
	 */
	HSQUIRRELVM GetVM() { return this->vm; }

	/**
	 * Load a script.
	 * @param script The full script-name to load.
	 * @return False if loading failed.
	 */
	bool LoadScript(const std::string &script);
	bool LoadScript(HSQUIRRELVM vm, const std::string &script, bool in_root = true);

	/**
	 * Load a file to a given VM.
	 * @param vm The virtual machine to load in.
	 * @param filename The file to open.
	 * @param printerror Whether to print errors, or completely ignore them.
	 * @return \c 0 when the file could be loaded, otherwise another number denoting an error.
	 */
	SQRESULT LoadFile(HSQUIRRELVM vm, const std::string &filename, SQBool printerror);

	/**
	 * Adds a function to the stack. Depending on the current state this means
	 *  either a method or a global function.
	 * @param method_name The name of the method.
	 * @param proc The actual method implementation to add.
	 * @param params Description of all the parameters.
	 * @param userdata Optional userdata to pass onto the created closure.
	 * @param size Number of bytes in the user data.
	 * @param suspendable Whether the method is suspendable.
	 */
	void AddMethod(std::string_view method_name, SQFUNCTION proc, std::string_view params = {}, void *userdata = nullptr, int size = 0, bool suspendable = false);

	/**
	 * Adds a const to the stack. Depending on the current state this means
	 *  either a const to a class or to the global space.
	 * @param var_name The name of the constant.
	 * @param value The value to set the constant to.
	 */
	void AddConst(std::string_view var_name, SQInteger value);

	/**
	 * Adds a const to the stack. Depending on the current state this means
	 *  either a const to a class or to the global space.
	 * @param var_name The name of the constant.
	 * @param value The value to set the constant to.
	 */
	void AddConst(std::string_view var_name, uint value) { this->AddConst(var_name, (SQInteger)value); }

	/**
	 * Adds a const to the stack. Depending on the current state this means
	 *  either a const to a class or to the global space.
	 * @param var_name The name of the constant.
	 * @param value The value to set the constant to.
	 */
	void AddConst(std::string_view var_name, int value) { this->AddConst(var_name, (SQInteger)value); }

	/**
	 * Adds a const to the stack. Depending on the current state this means
	 *  either a const to a class or to the global space.
	 * @param var_name The name of the constant.
	 * @param value The value to set the constant to.
	 */
	void AddConst(std::string_view var_name, const ConvertibleThroughBase auto &value) { this->AddConst(var_name, static_cast<SQInteger>(value.base())); }

	/**
	 * Adds a const to the stack. Depending on the current state this means
	 *  either a const to a class or to the global space.
	 * @param var_name The name of the constant.
	 * @param value The value to set the constant to.
	 */
	void AddConst(std::string_view var_name, bool value);

	/**
	 * Adds a class to the global scope. Make sure to call AddClassEnd when you
	 *  are done adding methods.
	 * @param class_name The name of the class.
	 */
	void AddClassBegin(std::string_view class_name);

	/**
	 * Adds a class to the global scope, extending 'parent_class'.
	 * Make sure to call AddClassEnd when you are done adding methods.
	 * @param class_name The name of the class.
	 * @param parent_class The name of the class that is being extended.
	 */
	void AddClassBegin(std::string_view class_name, std::string_view parent_class);

	/**
	 * Finishes adding a class to the global scope. If this isn't called, no
	 *  class is really created.
	 */
	void AddClassEnd();

	/**
	 * Resume a VM when it was suspended via a throw.
	 * @param suspend The number of opcodes that are allowed before suspending.
	 * @return \c true iff the VM is still alive.
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
	void InsertResult(ConvertibleThroughBase auto result) { this->InsertResult(static_cast<int>(result.base())); }

	/**
	 * Call a method of an instance returning a Squirrel object.
	 * @param instance The object to call the method on.
	 * @param method_name The name of the method to call.
	 * @param[out] ret The object to write the method result to.
	 * @param suspend Maximum number of operations until the method gets suspended.
	 * @return \c false iff the script crashed or returned a wrong type.
	 */
	bool CallMethod(HSQOBJECT instance, std::string_view method_name, HSQOBJECT *ret, int suspend);

	/**
	 * Call a method of an instance returning nothing.
	 * @param instance The object to call the method on.
	 * @param method_name The name of the method to call.
	 * @param suspend Maximum number of operations until the method gets suspended.
	 * @return \c false iff the script crashed or returned a wrong type.
	 */
	bool CallMethod(HSQOBJECT instance, std::string_view method_name, int suspend) { return this->CallMethod(instance, method_name, nullptr, suspend); }

	/**
	 * Call a method of an instance returning a string.
	 * @param instance The object to call the method on.
	 * @param method_name The name of the method to call.
	 * @param[out] res The result of calling the method.
	 * @param suspend Maximum number of operations until the method gets suspended.
	 * @return \c false iff the script crashed or returned a wrong type.
	 */
	bool CallStringMethod(HSQOBJECT instance, std::string_view method_name, std::string *res, int suspend);

	/**
	 * Call a method of an instance returning a integer.
	 * @param instance The object to call the method on.
	 * @param method_name The name of the method to call.
	 * @param[out] res The result of calling the method.
	 * @param suspend Maximum number of operations until the method gets suspended.
	 * @return \c false iff the script crashed or returned a wrong type.
	 */
	bool CallIntegerMethod(HSQOBJECT instance, std::string_view method_name, int *res, int suspend);

	/**
	 * Call a method of an instance returning a boolean.
	 * @param instance The object to call the method on.
	 * @param method_name The name of the method to call.
	 * @param[out] res The result of calling the method.
	 * @param suspend Maximum number of operations until the method gets suspended.
	 * @return \c false iff the script crashed or returned a wrong type.
	 */
	bool CallBoolMethod(HSQOBJECT instance, std::string_view method_name, bool *res, int suspend);

	/**
	 * Check if a method exists in an instance.
	 * @param instance The object to check.
	 * @param method_name The method to look for.
	 * @return \c true iff the object has the method.
	 */
	bool MethodExists(HSQOBJECT instance, std::string_view method_name);

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
	static bool CreateClassInstanceVM(HSQUIRRELVM vm, const std::string &class_name, void *real_instance, HSQOBJECT *instance, SQRELEASEHOOK release_hook, bool prepend_API_name = false);

	/**
	 * Create a class instance. Exactly the same as CreateClassInstanceVM, only callable without instance of Squirrel.
	 * @param class_name The name of the class of which we create an instance.
	 * @param real_instance The instance to the real class, if it represents a real class.
	 * @param instance Returning value with the pointer to the instance.
	 * @return \c true iff the class could be created.
	 */
	bool CreateClassInstance(const std::string &class_name, void *real_instance, HSQOBJECT *instance);

	/**
	 * Get the real-instance pointer.
	 * @note This will only work just after a function-call from within Squirrel
	 *  to your C++ function.
	 * @param vm The virtual machine to work in.
	 * @param index The location on the stack. A negative value is the location from the top.
	 * @param tag Name of the class without the Script/AI/GS moniker.
	 * @return The pointer.
	 */
	static SQUserPointer GetRealInstance(HSQUIRRELVM vm, int index, std::string_view tag);

	/**
	 * Get the Squirrel-instance pointer.
	 * @note This will only work just after a function-call from within Squirrel
	 *  to your C++ function.
	 * @param vm The virtual machine to work in.
	 * @param[out] ptr The pointer to write the object to.
	 * @param pos The position of the stack.
	 */
	static void GetInstance(HSQUIRRELVM vm, HSQOBJECT *ptr, int pos = 1) { sq_getclass(vm, pos); sq_getstackobj(vm, pos, ptr); sq_pop(vm, 1); }

	/**
	 * Convert a Squirrel-object to a string.
	 * @param ptr The object to convert.
	 * @return The \c std::string_view or \c std::nullopt.
	 */
	static std::optional<std::string_view> ObjectToString(HSQOBJECT *ptr) { return sq_objtostring(ptr); }

	/**
	 * Convert a Squirrel-object to an integer.
	 * @param ptr The object to convert.
	 * @return An integer.
	 */
	static int ObjectToInteger(HSQOBJECT *ptr) { return sq_objtointeger(ptr); }

	/**
	 * Convert a Squirrel-object to a bool.
	 * @param ptr The object to convert.
	 * @return A boolean.
	 */
	static bool ObjectToBool(HSQOBJECT *ptr) { return sq_objtobool(ptr) == 1; }

	/**
	 * Sets a pointer in the VM that is reachable from where ever you are in SQ.
	 *  Useful to keep track of the main instance.
	 * @param ptr Arbitrary pointer.
	 */
	void SetGlobalPointer(void *ptr) { this->global_pointer = ptr; }

	/**
	 * Get the pointer as set by SetGlobalPointer.
	 * @param vm The virtual machine to get the pointer for.
	 * @return The pointer previously set.
	 */
	static void *GetGlobalPointer(HSQUIRRELVM vm) { return ((Squirrel *)sq_getforeignptr(vm))->global_pointer; }

	/**
	 * Set a custom print function, so you can handle outputs from SQ yourself.
	 * @param func The print function.
	 */
	void SetPrintFunction(SQPrintFunc *func) { this->print_func = func; }

	/**
	 * Throw a Squirrel error that will be nicely displayed to the user.
	 * @param error The error to show to the user.
	 */
	void ThrowError(std::string_view error) { sq_throwerror(this->vm, error); }

	/**
	 * Release a SQ object.
	 * @param ptr The object to release.
	 */
	void ReleaseObject(HSQOBJECT *ptr) { sq_release(this->vm, ptr); }

	/**
	 * Tell the VM to remove \c amount ops from the number of ops till suspend.
	 * @param vm The virtual machine to change.
	 * @param amount The amount of operations to change by.
	 */
	static void DecreaseOps(HSQUIRRELVM vm, int amount);

	/**
	 * Did the squirrel code suspend or return normally.
	 * @return True if the function suspended.
	 */
	bool IsSuspended();

	/**
	 * Find out if the squirrel script made an error before.
	 * @return \c true iff \c CrashOccurred has been called.
	 */
	bool HasScriptCrashed();

	/**
	 * Set the script status to crashed.
	 */
	void CrashOccurred();

	/**
	 * Are we allowed to suspend the squirrel script at this moment?
	 * @return \c true iff it is safe to suspend the script.
	 */
	bool CanSuspend();

	/**
	 * How many operations can we execute till suspension?
	 * @return The number of operations left.
	 */
	SQInteger GetOpsTillSuspend();

	/**
	 * Completely reset the engine; start from scratch.
	 */
	void Reset();

	/**
	 * Get number of bytes allocated by this VM.
	 * @return The size in bytes.
	 */
	size_t GetAllocatedMemory() const noexcept;

	/**
	 * Increase number of bytes allocated in the current script allocator scope.
	 * @param bytes Number of bytes to increase.
	 */
	static void IncreaseAllocatedSize(size_t bytes);

	/**
	 * Decrease number of bytes allocated in the current script allocator scope.
	 * @param bytes Number of bytes to decrease.
	 */
	static void DecreaseAllocatedSize(size_t bytes);
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

	/** Restore the previous allocator. */
	~ScriptAllocatorScope()
	{
		_squirrel_allocator = this->old_allocator;
	}
};

#endif /* SQUIRREL_HPP */
