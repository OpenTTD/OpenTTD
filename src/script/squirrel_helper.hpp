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
#include "../core/smallvec_type.hpp"
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
	 * Pointers assigned to this class will be free'd when this instance
	 *  comes out of scope. Useful to make sure you can use stredup(),
	 *  without leaking memory.
	 */
	struct SQAutoFreePointers : std::vector<void *> {
		~SQAutoFreePointers()
		{
			for (void * p : *this) free(p);
		}
	};


	/**
	 * Special class to make it possible for the compiler to pick the correct GetParam().
	 */
	template <typename T> class ForceType { };

	/**
	 * To return a value to squirrel, we call this function. It converts to the right format.
	 */
	template <typename T> static int Return(HSQUIRRELVM vm, T t);

	template <> inline int Return<uint8>       (HSQUIRRELVM vm, uint8 res)       { sq_pushinteger(vm, (int32)res); return 1; }
	template <> inline int Return<uint16>      (HSQUIRRELVM vm, uint16 res)      { sq_pushinteger(vm, (int32)res); return 1; }
	template <> inline int Return<uint32>      (HSQUIRRELVM vm, uint32 res)      { sq_pushinteger(vm, (int32)res); return 1; }
	template <> inline int Return<int8>        (HSQUIRRELVM vm, int8 res)        { sq_pushinteger(vm, res); return 1; }
	template <> inline int Return<int16>       (HSQUIRRELVM vm, int16 res)       { sq_pushinteger(vm, res); return 1; }
	template <> inline int Return<int32>       (HSQUIRRELVM vm, int32 res)       { sq_pushinteger(vm, res); return 1; }
	template <> inline int Return<int64>       (HSQUIRRELVM vm, int64 res)       { sq_pushinteger(vm, res); return 1; }
	template <> inline int Return<Money>       (HSQUIRRELVM vm, Money res)       { sq_pushinteger(vm, res); return 1; }
	template <> inline int Return<TileIndex>   (HSQUIRRELVM vm, TileIndex res)   { sq_pushinteger(vm, (int32)res.value); return 1; }
	template <> inline int Return<bool>        (HSQUIRRELVM vm, bool res)        { sq_pushbool   (vm, res); return 1; }
	template <> inline int Return<char *>      (HSQUIRRELVM vm, char *res)       { if (res == nullptr) sq_pushnull(vm); else { sq_pushstring(vm, res, -1); free(res); } return 1; }
	template <> inline int Return<const char *>(HSQUIRRELVM vm, const char *res) { if (res == nullptr) sq_pushnull(vm); else { sq_pushstring(vm, res, -1); } return 1; }
	template <> inline int Return<void *>      (HSQUIRRELVM vm, void *res)       { sq_pushuserpointer(vm, res); return 1; }
	template <> inline int Return<HSQOBJECT>   (HSQUIRRELVM vm, HSQOBJECT res)   { sq_pushobject(vm, res); return 1; }

	/**
	 * To get a param from squirrel, we call this function. It converts to the right format.
	 */
	template <typename T> static T GetParam(ForceType<T>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr);

	template <> inline uint8       GetParam(ForceType<uint8>       , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; }
	template <> inline uint16      GetParam(ForceType<uint16>      , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; }
	template <> inline uint32      GetParam(ForceType<uint32>      , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; }
	template <> inline int8        GetParam(ForceType<int8>        , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; }
	template <> inline int16       GetParam(ForceType<int16>       , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; }
	template <> inline int32       GetParam(ForceType<int32>       , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; }
	template <> inline int64       GetParam(ForceType<int64>       , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; }
	template <> inline TileIndex   GetParam(ForceType<TileIndex>   , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return TileIndex((uint32)(int32)tmp); }
	template <> inline Money       GetParam(ForceType<Money>       , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger     tmp; sq_getinteger    (vm, index, &tmp); return tmp; }
	template <> inline bool        GetParam(ForceType<bool>        , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQBool        tmp; sq_getbool       (vm, index, &tmp); return tmp != 0; }
	template <> inline void       *GetParam(ForceType<void *>      , HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer tmp; sq_getuserpointer(vm, index, &tmp); return tmp; }
	template <> inline const char *GetParam(ForceType<const char *>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr)
	{
		/* Convert what-ever there is as parameter to a string */
		sq_tostring(vm, index);

		const SQChar *tmp;
		sq_getstring(vm, -1, &tmp);
		char *tmp_str = stredup(tmp);
		sq_poptop(vm);
		ptr->push_back((void *)tmp_str);
		StrMakeValidInPlace(tmp_str);
		return tmp_str;
	}

	template <> inline Array      *GetParam(ForceType<Array *>,      HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr)
	{
		/* Sanity check of the size. */
		if (sq_getsize(vm, index) > UINT16_MAX) throw sq_throwerror(vm, "an array used as parameter to a function is too large");

		SQObject obj;
		sq_getstackobj(vm, index, &obj);
		sq_pushobject(vm, obj);
		sq_pushnull(vm);

		std::vector<int32> data;

		while (SQ_SUCCEEDED(sq_next(vm, -2))) {
			SQInteger tmp;
			if (SQ_SUCCEEDED(sq_getinteger(vm, -1, &tmp))) {
				data.push_back((int32)tmp);
			} else {
				sq_pop(vm, 4);
				throw sq_throwerror(vm, "a member of an array used as parameter to a function is not numeric");
			}

			sq_pop(vm, 2);
		}
		sq_pop(vm, 2);

		Array *arr = (Array*)MallocT<byte>(sizeof(Array) + sizeof(int32) * data.size());
		arr->size = data.size();
		memcpy(arr->array, data.data(), sizeof(int32) * data.size());

		ptr->push_back(arr);
		return arr;
	}

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
		static int SQCall(void *instance, Tretval(*func)(Targs...), [[maybe_unused]] HSQUIRRELVM vm, std::index_sequence<i...>)
		{
			[[maybe_unused]] SQAutoFreePointers ptr;
			if constexpr (std::is_void_v<Tretval>) {
				(*func)(
					GetParam(ForceType<Targs>(), vm, 2 + i, &ptr)...
				);
				return 0;
			} else {
				Tretval ret = (*func)(
					GetParam(ForceType<Targs>(), vm, 2 + i, &ptr)...
				);
				return Return(vm, ret);
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
			[[maybe_unused]] SQAutoFreePointers ptr;
			if constexpr (std::is_void_v<Tretval>) {
				(instance->*func)(
					GetParam(ForceType<Targs>(), vm, 2 + i, &ptr)...
				);
				return 0;
			} else {
				Tretval ret = (instance->*func)(
					GetParam(ForceType<Targs>(), vm, 2 + i, &ptr)...
				);
				return Return(vm, ret);
			}
		}

		template <size_t... i>
		static Tcls *SQConstruct(Tcls *, Tretval(Tcls:: *func)(Targs...), [[maybe_unused]] HSQUIRRELVM vm, std::index_sequence<i...>)
		{
			[[maybe_unused]] SQAutoFreePointers ptr;
			Tcls *inst = new Tcls(
				GetParam(ForceType<Targs>(), vm, 2 + i, &ptr)...
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
	static SQInteger DefSQDestructorCallback(SQUserPointer p, SQInteger size)
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
