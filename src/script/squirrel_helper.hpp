/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file squirrel_helper.hpp declarations and parts of the implementation of the class for convert code */

#ifndef SQUIRREL_HELPER_HPP
#define SQUIRREL_HELPER_HPP

#include "squirrel.hpp"
#include "../core/alloc_func.hpp"
#include "../economy_type.h"
#include "../string_func.h"
#include "../tile_type.h"
#include "squirrel_helper_type.hpp"

template <class CL, ScriptType ST> const char *GetClassName();

/**
 * The Squirrel convert routines
 */
namespace SQConvert {
	/**
	 * To return a value to squirrel, we use this helper class. It converts to the right format.
	 * We use a class instead of a plain function to allow us to use partial template specializations.
	 */
	template <typename T> struct Return;

	template <> struct Return<uint8_t>        { static inline int Set(HSQUIRRELVM vm, uint8_t res)       { sq_pushinteger(vm, (int32_t)res); return 1; } };
	template <> struct Return<uint16_t>       { static inline int Set(HSQUIRRELVM vm, uint16_t res)      { sq_pushinteger(vm, (int32_t)res); return 1; } };
	template <> struct Return<uint32_t>       { static inline int Set(HSQUIRRELVM vm, uint32_t res)      { sq_pushinteger(vm, (int32_t)res); return 1; } };
	template <> struct Return<int8_t>         { static inline int Set(HSQUIRRELVM vm, int8_t res)        { sq_pushinteger(vm, res); return 1; } };
	template <> struct Return<int16_t>        { static inline int Set(HSQUIRRELVM vm, int16_t res)       { sq_pushinteger(vm, res); return 1; } };
	template <> struct Return<int32_t>        { static inline int Set(HSQUIRRELVM vm, int32_t res)       { sq_pushinteger(vm, res); return 1; } };
	template <> struct Return<int64_t>        { static inline int Set(HSQUIRRELVM vm, int64_t res)       { sq_pushinteger(vm, res); return 1; } };
	template <> struct Return<Money>        { static inline int Set(HSQUIRRELVM vm, Money res)       { sq_pushinteger(vm, res); return 1; } };
	template <> struct Return<TileIndex>    { static inline int Set(HSQUIRRELVM vm, TileIndex res)   { sq_pushinteger(vm, (int32_t)res.base()); return 1; } };
	template <> struct Return<bool>         { static inline int Set(HSQUIRRELVM vm, bool res)        { sq_pushbool   (vm, res); return 1; } };
	template <> struct Return<char *>       { /* Do not use char *, use std::optional<std::string> instead. */ };
	template <> struct Return<const char *> { /* Do not use const char *, use std::optional<std::string> instead. */ };
	template <> struct Return<void *>       { static inline int Set(HSQUIRRELVM vm, void *res)       { sq_pushuserpointer(vm, res); return 1; } };
	template <> struct Return<HSQOBJECT>    { static inline int Set(HSQUIRRELVM vm, HSQOBJECT res)   { sq_pushobject(vm, res); return 1; } };

	template <> struct Return<std::optional<std::string>> {
		static inline int Set(HSQUIRRELVM vm, std::optional<std::string> res)
		{
			if (res.has_value()) {
				sq_pushstring(vm, res.value(), -1);
			} else {
				sq_pushnull(vm);
			}
			return 1;
		}
	};

	/**
	 * To get a param from squirrel, we use this helper class. It converts to the right format.
	 * We use a class instead of a plain function to allow us to use partial template specializations.
	 */
	template <typename T> struct Param;

	template <> struct Param<uint8_t>        { static inline uint8_t       Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; } };
	template <> struct Param<uint16_t>       { static inline uint16_t      Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; } };
	template <> struct Param<uint32_t>       { static inline uint32_t      Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; } };
	template <> struct Param<int8_t>         { static inline int8_t        Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; } };
	template <> struct Param<int16_t>        { static inline int16_t       Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; } };
	template <> struct Param<int32_t>        { static inline int32_t       Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; } };
	template <> struct Param<int64_t>        { static inline int64_t       Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; } };
	template <> struct Param<TileIndex>    { static inline TileIndex   Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return TileIndex((uint32_t)(int32_t)tmp); } };
	template <> struct Param<Money>        { static inline Money       Get(HSQUIRRELVM vm, int index) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; } };
	template <> struct Param<bool>         { static inline bool        Get(HSQUIRRELVM vm, int index) { SQBool        tmp; sq_getbool       (vm, index, &tmp); return tmp != 0; } };
	template <> struct Param<const char *> { /* Do not use const char *, use std::string& instead. */ };
	template <> struct Param<void *>       { static inline void       *Get(HSQUIRRELVM vm, int index) { SQUserPointer tmp; sq_getuserpointer(vm, index, &tmp); return tmp; } };

	template <> struct Param<const std::string &> {
		static inline const std::string Get(HSQUIRRELVM vm, int index)
		{
			/* Convert what-ever there is as parameter to a string */
			sq_tostring(vm, index);

			const SQChar *tmp;
			sq_getstring(vm, -1, &tmp);
			std::string result = StrMakeValid(tmp);
			sq_poptop(vm);
			return result;
		}
	};

	template <typename Titem>
	struct Param<Array<Titem> &&> {
		static inline Array<Titem> Get(HSQUIRRELVM vm, int index)
		{
			/* Sanity check of the size. */
			if (sq_getsize(vm, index) > UINT16_MAX) throw sq_throwerror(vm, "an array used as parameter to a function is too large");

			SQObject obj;
			sq_getstackobj(vm, index, &obj);
			sq_pushobject(vm, obj);
			sq_pushnull(vm);

			Array<Titem> data;

			while (SQ_SUCCEEDED(sq_next(vm, -2))) {
				data.emplace_back(Param<Titem>::Get(vm, -1));
				sq_pop(vm, 2);
			}
			sq_pop(vm, 2);

			return data;
		}
	};

	/**
	 * Helper class to recognize the function type (retval type, args) and use the proper specialization
	 * for SQ callback. The partial specializations for the second arg (Tis_void_retval) are not possible
	 * on the function. Therefore the class is used instead.
	 */
	template <typename Tfunc> struct HelperT;

	/**
	 * The real C++ caller for functions.
	 */
	template <typename Tretval, typename... Targs>
	struct HelperT<Tretval (*)(Targs...)> {
		static int SQCall(void *instance, Tretval(*func)(Targs...), HSQUIRRELVM vm)
		{
			return SQCall(instance, func, vm, std::index_sequence_for<Targs...>{});
		}

	private:
		template <size_t... i>
		static int SQCall(void *, Tretval(*func)(Targs...), [[maybe_unused]] HSQUIRRELVM vm, std::index_sequence<i...>)
		{
			if constexpr (std::is_void_v<Tretval>) {
				(*func)(
					Param<Targs>::Get(vm, 2 + i)...
				);
				return 0;
			} else {
				Tretval ret = (*func)(
					Param<Targs>::Get(vm, 2 + i)...
				);
				return Return<Tretval>::Set(vm, ret);
			}
		}
	};

	/**
	 * The real C++ caller for methods.
	 */
	template <class Tcls, typename Tretval, typename... Targs>
	struct HelperT<Tretval(Tcls:: *)(Targs...)> {
		static int SQCall(Tcls *instance, Tretval(Tcls:: *func)(Targs...), HSQUIRRELVM vm)
		{
			return SQCall(instance, func, vm, std::index_sequence_for<Targs...>{});
		}

		static Tcls *SQConstruct(Tcls *instance, Tretval(Tcls:: *func)(Targs...), HSQUIRRELVM vm)
		{
			return SQConstruct(instance, func, vm, std::index_sequence_for<Targs...>{});
		}

	private:
		template <size_t... i>
		static int SQCall(Tcls *instance, Tretval(Tcls:: *func)(Targs...), [[maybe_unused]] HSQUIRRELVM vm, std::index_sequence<i...>)
		{
			if constexpr (std::is_void_v<Tretval>) {
				(instance->*func)(
					Param<Targs>::Get(vm, 2 + i)...
				);
				return 0;
			} else {
				Tretval ret = (instance->*func)(
					Param<Targs>::Get(vm, 2 + i)...
				);
				return Return<Tretval>::Set(vm, ret);
			}
		}

		template <size_t... i>
		static Tcls *SQConstruct(Tcls *, Tretval(Tcls:: *)(Targs...), [[maybe_unused]] HSQUIRRELVM vm, std::index_sequence<i...>)
		{
			Tcls *inst = new Tcls(
				Param<Targs>::Get(vm, 2 + i)...
			);

			return inst;
		}
	};


	/**
	 * A general template for all non-static method callbacks from Squirrel.
	 *  In here the function_proc is recovered, and the SQCall is called that
	 *  can handle this exact amount of params.
	 */
	template <typename Tcls, typename Tmethod, ScriptType Ttype>
	inline SQInteger DefSQNonStaticCallback(HSQUIRRELVM vm)
	{
		/* Find the amount of params we got */
		int nparam = sq_gettop(vm);
		SQUserPointer ptr = nullptr;
		SQUserPointer real_instance = nullptr;
		HSQOBJECT instance;

		/* Get the 'SQ' instance of this class */
		Squirrel::GetInstance(vm, &instance);

		/* Protect against calls to a non-static method in a static way */
		sq_pushroottable(vm);
		const char *className = GetClassName<Tcls, Ttype>();
		sq_pushstring(vm, className, -1);
		sq_get(vm, -2);
		sq_pushobject(vm, instance);
		if (sq_instanceof(vm) != SQTrue) return sq_throwerror(vm, "class method is non-static");
		sq_pop(vm, 3);

		/* Get the 'real' instance of this class */
		sq_getinstanceup(vm, 1, &real_instance, nullptr);
		/* Get the real function pointer */
		sq_getuserdata(vm, nparam, &ptr, nullptr);
		if (real_instance == nullptr) return sq_throwerror(vm, "couldn't detect real instance of class for non-static call");
		/* Remove the userdata from the stack */
		sq_pop(vm, 1);

		try {
			/* Delegate it to a template that can handle this specific function */
			return HelperT<Tmethod>::SQCall((Tcls *)real_instance, *(Tmethod *)ptr, vm);
		} catch (SQInteger &e) {
			return e;
		}
	}

	/**
	 * A general template for all non-static advanced method callbacks from Squirrel.
	 *  In here the function_proc is recovered, and the SQCall is called that
	 *  can handle this exact amount of params.
	 */
	template <typename Tcls, typename Tmethod, ScriptType Ttype>
	inline SQInteger DefSQAdvancedNonStaticCallback(HSQUIRRELVM vm)
	{
		/* Find the amount of params we got */
		int nparam = sq_gettop(vm);
		SQUserPointer ptr = nullptr;
		SQUserPointer real_instance = nullptr;
		HSQOBJECT instance;

		/* Get the 'SQ' instance of this class */
		Squirrel::GetInstance(vm, &instance);

		/* Protect against calls to a non-static method in a static way */
		sq_pushroottable(vm);
		const char *className = GetClassName<Tcls, Ttype>();
		sq_pushstring(vm, className, -1);
		sq_get(vm, -2);
		sq_pushobject(vm, instance);
		if (sq_instanceof(vm) != SQTrue) return sq_throwerror(vm, "class method is non-static");
		sq_pop(vm, 3);

		/* Get the 'real' instance of this class */
		sq_getinstanceup(vm, 1, &real_instance, nullptr);
		/* Get the real function pointer */
		sq_getuserdata(vm, nparam, &ptr, nullptr);
		if (real_instance == nullptr) return sq_throwerror(vm, "couldn't detect real instance of class for non-static call");
		/* Remove the userdata from the stack */
		sq_pop(vm, 1);

		/* Call the function, which its only param is always the VM */
		return (SQInteger)(((Tcls *)real_instance)->*(*(Tmethod *)ptr))(vm);
	}

	/**
	 * A general template for all function/static method callbacks from Squirrel.
	 *  In here the function_proc is recovered, and the SQCall is called that
	 *  can handle this exact amount of params.
	 */
	template <typename Tcls, typename Tmethod>
	inline SQInteger DefSQStaticCallback(HSQUIRRELVM vm)
	{
		/* Find the amount of params we got */
		int nparam = sq_gettop(vm);
		SQUserPointer ptr = nullptr;

		/* Get the real function pointer */
		sq_getuserdata(vm, nparam, &ptr, nullptr);

		try {
			/* Delegate it to a template that can handle this specific function */
			return HelperT<Tmethod>::SQCall((Tcls *)nullptr, *(Tmethod *)ptr, vm);
		} catch (SQInteger &e) {
			return e;
		}
	}


	/**
	 * A general template for all static advanced method callbacks from Squirrel.
	 *  In here the function_proc is recovered, and the SQCall is called that
	 *  can handle this exact amount of params.
	 */
	template <typename Tcls, typename Tmethod>
	inline SQInteger DefSQAdvancedStaticCallback(HSQUIRRELVM vm)
	{
		/* Find the amount of params we got */
		int nparam = sq_gettop(vm);
		SQUserPointer ptr = nullptr;

		/* Get the real function pointer */
		sq_getuserdata(vm, nparam, &ptr, nullptr);
		/* Remove the userdata from the stack */
		sq_pop(vm, 1);

		/* Call the function, which its only param is always the VM */
		return (SQInteger)(*(*(Tmethod *)ptr))(vm);
	}

	/**
	 * A general template for the destructor of SQ instances. This is needed
	 *  here as it has to be in the same scope as DefSQConstructorCallback.
	 */
	template <typename Tcls>
	static SQInteger DefSQDestructorCallback(SQUserPointer p, SQInteger)
	{
		/* Remove the real instance too */
		if (p != nullptr) ((Tcls *)p)->Release();
		return 0;
	}

	/**
	 * A general template to handle creating of instance with any amount of
	 *  params. It creates the instance in C++, and it sets all the needed
	 *  settings in SQ to register the instance.
	 */
	template <typename Tcls, typename Tmethod, int Tnparam>
	inline SQInteger DefSQConstructorCallback(HSQUIRRELVM vm)
	{
		try {
			/* Create the real instance */
			Tcls *instance = HelperT<Tmethod>::SQConstruct((Tcls *)nullptr, (Tmethod)nullptr, vm);
			sq_setinstanceup(vm, -Tnparam, instance);
			sq_setreleasehook(vm, -Tnparam, DefSQDestructorCallback<Tcls>);
			instance->AddRef();
			return 0;
		} catch (SQInteger &e) {
			return e;
		}
	}

	/**
	 * A general template to handle creating of an instance with a complex
	 *  constructor.
	 */
	template <typename Tcls>
	inline SQInteger DefSQAdvancedConstructorCallback(HSQUIRRELVM vm)
	{
		try {
			/* Find the amount of params we got */
			int nparam = sq_gettop(vm);

			/* Create the real instance */
			Tcls *instance = new Tcls(vm);
			sq_setinstanceup(vm, -nparam, instance);
			sq_setreleasehook(vm, -nparam, DefSQDestructorCallback<Tcls>);
			instance->AddRef();
			return 0;
		} catch (SQInteger &e) {
			return e;
		}
	}

} // namespace SQConvert

#endif /* SQUIRREL_HELPER_HPP */
