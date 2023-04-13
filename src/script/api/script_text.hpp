/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_text.hpp Everything to handle text which can be translated. */

#ifndef SCRIPT_TEXT_HPP
#define SCRIPT_TEXT_HPP

#include "script_object.hpp"
#include "../../core/alloc_type.hpp"

#include <variant>

/**
 * Internal parent object of all Text-like objects.
 * @api -all
 */
class Text : public ScriptObject {
public:
	/**
	 * Convert a ScriptText to a normal string.
	 * @return A string.
	 * @api -all
	 */
	virtual std::string GetEncodedText() = 0;

	/**
	 * Convert a #ScriptText into a decoded normal string.
	 * @return A string.
	 * @api -all
	 */
	const std::string GetDecodedText();
};

/**
 * Internally used class to create a raw text in a Text object.
 * @api -all
 */
class RawText : public Text {
public:
	RawText(const std::string &text);

	std::string GetEncodedText() override { return this->text; }
private:
	const std::string text;
};

/**
 * Class that handles all text related functions. You can define a language
 *  file in lang/english.txt, in the same format as OpenTTD does, including
 *  tags like {BLACK}, {STRING1} etc. The name given to this string is made
 *  available to you in ScriptText, for example: ScriptText.STR_NEWS, if your
 *  english.txt contains: STR_NEWS    :{BLACK}Welcome {COMPANY}!
 *
 * In translation files like lang/dutch.txt you can then translate such
 *  strings, like: STR_NEWS    :{BLACK}Hallo {COMPANY}!
 * When the user has the dutch language selected, it will automatically use
 *  the translated string when available. The fallback language is always
 *  the english language.
 *
 * If you use parameters in your strings, you will have to define those
 *  parameters, for example like this:
 * \code local text = ScriptText(ScriptText.STR_NEWS);
 * text.AddParam(1); \endcode
 * This will set the {COMPANY} to the name of Company 1. Alternatively you
 *  can directly give those arguments to the ScriptText constructor, like this:
 * \code local text = ScriptText(ScriptText.STR_NEWS, 1); \endcode
 *
 * @api game
 */
class ScriptText : public Text , public ZeroedMemoryAllocator {
public:
	static const int SCRIPT_TEXT_MAX_PARAMETERS = 20; ///< The maximum amount of parameters you can give to one object.

#ifndef DOXYGEN_API
	/**
	 * The constructor wrapper from Squirrel.
	 */
	ScriptText(HSQUIRRELVM vm);
#else
	/**
	 * Generate a text from string. You can set parameters to the instance which
	 *  can be required for the string.
	 * @param string The string of the text.
	 * @param ... Optional arguments for this string.
	 */
	ScriptText(StringID string, ...);
#endif /* DOXYGEN_API */

#ifndef DOXYGEN_API
	/**
	 * Used for .param_N and [] set from Squirrel.
	 */
	SQInteger _set(HSQUIRRELVM vm);

	/**
	 * Set the parameter.
	 */
	SQInteger SetParam(HSQUIRRELVM vm);

	/**
	 * Add an parameter
	 */
	SQInteger AddParam(HSQUIRRELVM vm);
#else
	/**
	 * Set the parameter to a value.
	 * @param parameter Which parameter to set.
	 * @param value The value of the parameter. Has to be string, integer or an instance of the class ScriptText.
	 */
	void SetParam(int parameter, Object value);

	/**
	 * Add a value as parameter (appending it).
	 * @param value The value of the parameter. Has to be string, integer or an instance of the class ScriptText.
	 * @return The same object as on which this is called, so you can chain.
	 */
	ScriptText *AddParam(Object value);
#endif /* DOXYGEN_API */

	/**
	 * @api -all
	 */
	std::string GetEncodedText() override;

private:
	using ScriptTextRef = ScriptObjectRef<ScriptText>;
	using StringIDList = std::vector<StringID>;

	StringID string;
	std::variant<SQInteger, std::string, ScriptTextRef> param[SCRIPT_TEXT_MAX_PARAMETERS];
	int paramc;

	/**
	 * Internal function for recursive calling this function over multiple
	 *  instances, while writing in the same buffer.
	 * @param output The output to write the encoded text to.
	 * @param param_count The number of parameters that are in the string.
	 * @param seen_ids The list of seen StringID.
	 */
	void _GetEncodedText(std::back_insert_iterator<std::string> &output, int &param_count, StringIDList &seen_ids);

	/**
	 * Set a parameter, where the value is the first item on the stack.
	 */
	SQInteger _SetParam(int k, HSQUIRRELVM vm);
};

#endif /* SCRIPT_TEXT_HPP */
