/* $Id$ */

/** @file squirrel_class.cpp defines templates for converting C++ classes to Squirrel classes */

#ifndef SQUIRREL_CLASS_HPP
#define SQUIRREL_CLASS_HPP

#if (__GNUC__ == 2)
/* GCC 2.95 doesn't like to have SQConvert::DefSQStaticCallback inside a
 *  template (it gives an internal error 373). Above that, it doesn't listen
 *  to 'using namespace' inside a function of a template. So for GCC 2.95 we
 *  do it in the global space to avoid compiler errors. */
using namespace SQConvert;
#endif /* __GNUC__ == 2 */

/**
 * The template to define classes in Squirrel. It takes care of the creation
 *  and calling of such classes, to make the AI Layer cleaner while having a
 *  powerful script as possible AI language.
 */
template <class CL>
class DefSQClass {
private:
	const char *classname;

public:
	DefSQClass(const char *_classname) :
		classname(_classname)
	{}

	/**
	 * This defines a method inside a class for Squirrel.
	 */
	template <typename Func>
	void DefSQMethod(Squirrel *engine, Func function_proc, const char *function_name)
	{
		using namespace SQConvert;
		engine->AddMethod(function_name, DefSQNonStaticCallback<CL, Func>, 0, NULL, &function_proc, sizeof(function_proc));
	}

	/**
	 * This defines a method inside a class for Squirrel, which has access to the 'engine' (experts only!).
	 */
	template <typename Func>
	void DefSQAdvancedMethod(Squirrel *engine, Func function_proc, const char *function_name)
	{
		using namespace SQConvert;
		engine->AddMethod(function_name, DefSQAdvancedNonStaticCallback<CL, Func>, 0, NULL, &function_proc, sizeof(function_proc));
	}

	/**
	 * This defines a method inside a class for Squirrel with defined params.
	 * @note If you define nparam, make sure that he first param is always 'x',
	 *  which is the 'this' inside the function. This is hidden from the rest
	 *  of the code, but without it calling your function will fail!
	 */
	template <typename Func>
	void DefSQMethod(Squirrel *engine, Func function_proc, const char *function_name, int nparam, const char *params)
	{
		using namespace SQConvert;
		engine->AddMethod(function_name, DefSQNonStaticCallback<CL, Func>, nparam, params, &function_proc, sizeof(function_proc));
	}

	/**
	 * This defines a static method inside a class for Squirrel.
	 */
	template <typename Func>
	void DefSQStaticMethod(Squirrel *engine, Func function_proc, const char *function_name)
	{
		using namespace SQConvert;
		engine->AddMethod(function_name, DefSQStaticCallback<CL, Func>, 0, NULL, &function_proc, sizeof(function_proc));
	}

	/**
	 * This defines a static method inside a class for Squirrel with defined params.
	 * @note If you define nparam, make sure that he first param is always 'x',
	 *  which is the 'this' inside the function. This is hidden from the rest
	 *  of the code, but without it calling your function will fail!
	 */
	template <typename Func>
	void DefSQStaticMethod(Squirrel *engine, Func function_proc, const char *function_name, int nparam, const char *params)
	{
		using namespace SQConvert;
		engine->AddMethod(function_name, DefSQStaticCallback<CL, Func>, nparam, params, &function_proc, sizeof(function_proc));
	}

	template <typename Var>
	void DefSQConst(Squirrel *engine, Var value, const char *var_name)
	{
		engine->AddConst(var_name, value);
	}

	void PreRegister(Squirrel *engine)
	{
		engine->AddClassBegin(this->classname);
	}

	void PreRegister(Squirrel *engine, const char *parent_class)
	{
		engine->AddClassBegin(this->classname, parent_class);
	}

	template <typename Func, int Tnparam>
	void AddConstructor(Squirrel *engine, const char *params)
	{
		using namespace SQConvert;
		engine->AddMethod("constructor", DefSQConstructorCallback<CL, Func, Tnparam>, Tnparam, params);
	}

	void PostRegister(Squirrel *engine)
	{
		engine->AddClassEnd();
	}
};

#endif /* SQUIRREL_CLASS_HPP */
