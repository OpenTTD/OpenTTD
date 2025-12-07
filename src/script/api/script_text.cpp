/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_text.cpp Implementation of ScriptText. */

#include "../../stdafx.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../game/game_text.hpp"
#include "script_text.hpp"
#include "script_log.hpp"
#include "../script_fatalerror.hpp"
#include "../../core/string_consumer.hpp"
#include "../../table/control_codes.h"

#include "table/strings.h"

#include "../../safeguards.h"

EncodedString RawText::GetEncodedText()
{
	return ::GetEncodedString(STR_JUST_RAW_STRING, this->text);
}

ScriptText::ScriptText(HSQUIRRELVM vm)
{
	int nparam = sq_gettop(vm) - 1;
	if (nparam < 1) {
		throw sq_throwerror(vm, "You need to pass at least a StringID to the constructor");
	}

	/* First resolve the StringID. */
	SQInteger sqstring;
	if (SQ_FAILED(sq_getinteger(vm, 2, &sqstring))) {
		throw sq_throwerror(vm, "First argument must be a valid StringID");
	}
	this->string = StringIndexInTab(sqstring);

	/* The rest of the parameters must be arguments. */
	for (int i = 0; i < nparam - 1; i++) {
		/* Push the parameter to the top of the stack. */
		sq_push(vm, i + 3);

		if (SQ_FAILED(this->_SetParam(i, vm))) {
			this->~ScriptText();
			throw sq_throwerror(vm, "Invalid parameter");
		}

		/* Pop the parameter again. */
		sq_pop(vm, 1);
	}
}

SQInteger ScriptText::_SetParam(int parameter, HSQUIRRELVM vm)
{
	if (static_cast<size_t>(parameter) >= std::size(this->param)) this->param.resize(parameter + 1);

	switch (sq_gettype(vm, -1)) {
		case OT_STRING: {
			std::string_view view;
			sq_getstring(vm, -1, view);

			this->param[parameter] = StrMakeValid(view);
			break;
		}

		case OT_INTEGER: {
			SQInteger value;
			sq_getinteger(vm, -1, &value);

			this->param[parameter] = value;
			break;
		}

		case OT_INSTANCE: {
			SQUserPointer real_instance = nullptr;
			HSQOBJECT instance;

			sq_getstackobj(vm, -1, &instance);

			/* Validate if it is a GSText instance */
			sq_pushroottable(vm);
			sq_pushstring(vm, "GSText");
			sq_get(vm, -2);
			sq_pushobject(vm, instance);
			if (sq_instanceof(vm) != SQTrue) return SQ_ERROR;
			sq_pop(vm, 3);

			/* Get the 'real' instance of this class */
			sq_getinstanceup(vm, -1, &real_instance, nullptr);
			if (real_instance == nullptr) return SQ_ERROR;

			ScriptText *value = static_cast<ScriptText *>(real_instance);
			this->param[parameter] = ScriptTextRef(value);
			break;
		}

		case OT_NULL:
			this->param[parameter] = {};
			break;

		default: return SQ_ERROR;
	}

	return 0;
}

SQInteger ScriptText::SetParam(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;

	SQInteger k;
	sq_getinteger(vm, 2, &k);

	if (k < 1) return SQ_ERROR;
	k--;

	return this->_SetParam(k, vm);
}

SQInteger ScriptText::AddParam(HSQUIRRELVM vm)
{
	SQInteger res;
	res = this->_SetParam(static_cast<int>(std::size(this->param)), vm);
	if (res != 0) return res;

	/* Push our own instance back on top of the stack */
	sq_push(vm, 1);
	return 1;
}

SQInteger ScriptText::_set(HSQUIRRELVM vm)
{
	int32_t k;

	if (sq_gettype(vm, 2) == OT_STRING) {
		std::string_view view;
		sq_getstring(vm, 2, view);

		std::string str = StrMakeValid(view);
		if (!str.starts_with("param_")) return SQ_ERROR;

		auto key = ParseInteger<int32_t>(str.substr(6));
		if (!key.has_value()) return SQ_ERROR;
		k = *key;
	} else if (sq_gettype(vm, 2) == OT_INTEGER) {
		SQInteger key;
		sq_getinteger(vm, 2, &key);
		k = (int32_t)key;
	} else {
		return SQ_ERROR;
	}

	if (k < 1) return SQ_ERROR;
	k--;

	return this->_SetParam(k, vm);
}

/**
 * Set the number of padding parameters to use, for compatibility with old scripts.
 * This is called during RegisterGameTranslation.
 */
void ScriptText::SetPadParameterCount(HSQUIRRELVM vm)
{
	ScriptText::pad_parameter_count = 0;

	SQInteger top = sq_gettop(vm);
	sq_pushroottable(vm);
	sq_pushstring(vm, "GSText");
	if (!SQ_FAILED(sq_get(vm, -2))) {
		sq_pushstring(vm, "SCRIPT_TEXT_MAX_PARAMETERS");
		if (!SQ_FAILED(sq_get(vm, -2))) {
			SQInteger value;
			if (!SQ_FAILED(sq_getinteger(vm, -1, &value))) {
				ScriptText::pad_parameter_count = value;
			}
		}
	}
	sq_settop(vm, top);
}

EncodedString ScriptText::GetEncodedText()
{
	ScriptTextList seen_texts;
	ParamList params;
	int param_count = 0;
	std::string result;
	StringBuilder builder(result);
	this->_FillParamList(params, seen_texts);
	this->_GetEncodedText(builder, param_count, params, true);
	return ::EncodedString{std::move(result)};
}

void ScriptText::_FillParamList(ParamList &params, ScriptTextList &seen_texts)
{
	if (std::ranges::find(seen_texts, this) != seen_texts.end()) throw Script_FatalError(fmt::format("{}: Circular reference detected", GetGameStringName(this->string)));
	seen_texts.push_back(this);

	for (int idx = 0; Param &p : this->param) {
		params.emplace_back(this->string, idx, &p);
		++idx;
		if (!std::holds_alternative<ScriptTextRef>(p)) continue;
		std::get<ScriptTextRef>(p)->_FillParamList(params, seen_texts);
	}

	seen_texts.pop_back();

	/* Fill with dummy parameters to match old FormatString() compatibility behaviour. */
	if (seen_texts.empty() && ScriptText::pad_parameter_count > 0) {
		static Param dummy = {};
		for (int idx = static_cast<int>(std::size(this->param)); idx < ScriptText::pad_parameter_count; ++idx) {
			params.emplace_back(StringIndexInTab(-1), idx, &dummy);
		}
	}
}

void ScriptText::ParamCheck::Encode(StringBuilder &builder, std::string_view cmd)
{
	if (this->cmd.empty()) this->cmd = cmd;
	if (this->used) return;

	struct visitor {
		StringBuilder &builder;

		void operator()(const std::monostate &) { }

		void operator()(std::string value)
		{
			this->builder.PutUtf8(SCC_ENCODED_STRING);
			StrMakeValidInPlace(value, {StringValidationSetting::ReplaceWithQuestionMark, StringValidationSetting::AllowNewline, StringValidationSetting::ReplaceTabCrNlWithSpace});
			this->builder.Put(value);
		}

		void operator()(const SQInteger &value)
		{
			this->builder.PutUtf8(SCC_ENCODED_NUMERIC);
			/* Sign-extend the value, then store as unsigned */
			this->builder.PutIntegerBase<uint64_t>(static_cast<uint64_t>(static_cast<int64_t>(value)), 16);
		}

		void operator()(const ScriptTextRef &value)
		{
			this->builder.PutUtf8(SCC_ENCODED);
			this->builder.PutIntegerBase(value->string.base(), 16);
		}
	};

	builder.PutUtf8(SCC_RECORD_SEPARATOR);
	std::visit(visitor{builder}, *this->param);
	this->used = true;
}

void ScriptText::_GetEncodedText(StringBuilder &builder, int &param_count, ParamSpan args, bool first)
{
	const std::string &name = GetGameStringName(this->string);

	if (first) {
		builder.PutUtf8(SCC_ENCODED);
		builder.PutIntegerBase(this->string.base(), 16);
	}

	const StringParams &params = GetGameStringParams(this->string);

	size_t idx = 0;
	auto get_next_arg = [&]() {
		if (idx >= args.size()) throw Script_FatalError(fmt::format("{}({}): Not enough parameters", name, param_count + 1));
		ParamCheck &pc = args[idx++];
		if (pc.owner != this->string) ScriptLog::Warning(fmt::format("{}({}): Consumes {}({})", name, param_count + 1, GetGameStringName(pc.owner), pc.idx + 1));
		return &pc;
	};
	auto skip_args = [&](size_t nb) { idx += nb; };

	for (const StringParam &cur_param : params) {
		try {
			switch (cur_param.type) {
				case StringParam::UNUSED:
					skip_args(cur_param.consumes);
					break;

				case StringParam::RAW_STRING:
				{
					ParamCheck &p = *get_next_arg();
					p.Encode(builder, cur_param.cmd);
					if (p.cmd != cur_param.cmd) throw 1;
					if (!std::holds_alternative<std::string>(*p.param)) ScriptLog::Error(fmt::format("{}({}): {{{}}} expects a raw string", name, param_count + 1, cur_param.cmd));
					break;
				}

				case StringParam::STRING:
				{
					ParamCheck &p = *get_next_arg();
					p.Encode(builder, cur_param.cmd);
					if (p.cmd != cur_param.cmd) throw 1;
					if (!std::holds_alternative<ScriptTextRef>(*p.param)) {
						ScriptLog::Error(fmt::format("{}({}): {{{}}} expects a GSText", name, param_count + 1, cur_param.cmd));
						param_count++;
						continue;
					}
					int count = 0;
					ScriptTextRef &ref = std::get<ScriptTextRef>(*p.param);
					ref->_GetEncodedText(builder, count, args.subspan(idx), false);
					if (++count != cur_param.consumes) {
						ScriptLog::Warning(fmt::format("{}({}): {{{}}} expects {} to be consumed, but {} consumes {}", name, param_count + 1, cur_param.cmd, cur_param.consumes - 1, GetGameStringName(ref->string), count - 1));
						/* Fill missing params if needed. */
						for (int i = count; i < cur_param.consumes; i++) {
							builder.PutUtf8(SCC_RECORD_SEPARATOR);
						}
					}
					skip_args(cur_param.consumes - 1);
					break;
				}

				default:
					for (int i = 0; i < cur_param.consumes; i++) {
						ParamCheck &p = *get_next_arg();
						p.Encode(builder, i == 0 ? cur_param.cmd : "");
						if (i == 0 && p.cmd != cur_param.cmd) throw 1;
						if (!std::holds_alternative<SQInteger>(*p.param)) ScriptLog::Error(fmt::format("{}({}): {{{}}} expects an integer", name, param_count + i + 1, cur_param.cmd));
					}
			}

			param_count += cur_param.consumes;
		} catch (int nb) {
			param_count += nb;
			ScriptLog::Warning(fmt::format("{}({}): Invalid parameter", name, param_count));
		}
	}
}

const std::string Text::GetDecodedText()
{
	return this->GetEncodedText().GetDecodedString();
}
