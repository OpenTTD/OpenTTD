/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file squirrel_class.hpp Defines templates for converting C++ classes to Squirrel classes */

#ifndef SQUIRREL_CLASS_HPP
#define SQUIRREL_CLASS_HPP

#include "squirrel_helper.hpp"

/**
 * The template to define classes in Squirrel. It takes care of the creation
 *  and calling of such classes, to minimize the API layer.
 */
template <class CL, ScriptType ST>
class DefSQClass {
private:
	std::string_view classname;

public:
	DefSQClass(std::string_view _classname) :
		classname(_classname)
	{}

	/**
	 * This defines a method inside a class for Squirrel with defined params.
	 * @note If you define params, make sure that the first param is always 'x',
	 *  which is the 'this' inside the function. This is hidden from the rest
	 *  of the code, but without it calling your function will fail!
	 */
	template <typename Func>
	void DefSQMethod(Squirrel &engine, Func function_proc, std::string_view function_name, std::string_view params = {})
	{
		using namespace SQConvert;
		engine.AddMethod(function_name, DefSQNonStaticCallback<CL, Func, ST>, params, &function_proc, sizeof(function_proc));
	}

	/**
	 * This defines a method inside a class for Squirrel, which has access to the 'engine' (experts only!).
	 */
	template <typename Func>
	void DefSQAdvancedMethod(Squirrel &engine, Func function_proc, std::string_view function_name)
	{
		using namespace SQConvert;
		engine.AddMethod(function_name, DefSQAdvancedNonStaticCallback<CL, Func, ST>, {}, &function_proc, sizeof(function_proc));
	}

	/**
	 * This defines a static method inside a class for Squirrel with defined params.
	 * @note If you define params, make sure that the first param is always 'x',
	 *  which is the 'this' inside the function. This is hidden from the rest
	 *  of the code, but without it calling your function will fail!
	 */
	template <typename Func>
	void DefSQStaticMethod(Squirrel &engine, Func function_proc, std::string_view function_name, std::string_view params = {})
	{
		using namespace SQConvert;
		engine.AddMethod(function_name, DefSQStaticCallback<CL, Func>, params, &function_proc, sizeof(function_proc));
	}

	/**
	 * This defines a static method inside a class for Squirrel, which has access to the 'engine' (experts only!).
	 */
	template <typename Func>
	void DefSQAdvancedStaticMethod(Squirrel &engine, Func function_proc, std::string_view function_name)
	{
		using namespace SQConvert;
		engine.AddMethod(function_name, DefSQAdvancedStaticCallback<CL, Func>, {}, &function_proc, sizeof(function_proc));
	}

	template <typename Var>
	void DefSQConst(Squirrel &engine, Var value, std::string_view var_name)
	{
		engine.AddConst(var_name, value);
	}

	void PreRegister(Squirrel &engine)
	{
		engine.AddClassBegin(this->classname);
	}

	void PreRegister(Squirrel &engine, std::string_view parent_class)
	{
		engine.AddClassBegin(this->classname, parent_class);
	}

	template <typename Func>
	void AddConstructor(Squirrel &engine, std::string_view params)
	{
		using namespace SQConvert;
		engine.AddMethod("constructor", DefSQConstructorCallback<CL, Func>, params);
	}

	void AddSQAdvancedConstructor(Squirrel &engine)
	{
		using namespace SQConvert;
		engine.AddMethod("constructor", DefSQAdvancedConstructorCallback<CL>);
	}

	void PostRegister(Squirrel &engine)
	{
		engine.AddClassEnd();
	}
};

#endif /* SQUIRREL_CLASS_HPP */
