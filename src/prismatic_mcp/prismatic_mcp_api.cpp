/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE.
 */

/**
 * @file prismatic_mcp_api.cpp The Script API as MCP tools (I0036).
 *
 * A private Game Script engine, never run as a script, holds the whole Game Script API exactly as a
 * game script sees it: GameInstance's own registration (SQGS_RegisterAll, generated from
 * src/script/api/script_*.hpp at build time). The tool catalogue is read back from that engine: every
 * class GS<Name> becomes Script<Name>, every native static function in it a tool, its parameter types
 * the engine's own parameter checks. A call goes through the same binding a script's call goes
 * through, so argument conversion, validity checks and error codes are the game's.
 *
 * Its storage (last error, cost, modes) is its own, so a call never disturbs a running AI or game
 * script. It runs as OWNER_DEITY unless the call names a company (GSCompanyMode). Without
 * `execute` every call runs in GSTestMode, where a function that issues a command only estimates
 * it; `execute` goes through the approval gate and runs in GSAsyncMode, so the command is posted
 * without suspending a script that is not running.
 */

#include "../stdafx.h"

#include "prismatic_mcp_api.h"

#include "../script/script_instance.hpp"
#include "../script/script_fatalerror.hpp"
#include "../script/script_suspend.hpp"
#include "../script/squirrel.hpp"
#include "../script/api/script_object.hpp"
#include "../script/api/script_companymode.hpp"
#include "../script/api/script_error.hpp"
#include "../script/api/script_list.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../core/backup_type.hpp"
#include "../company_type.h"

#include <../squirrel/sqpcheader.h>
#include <../squirrel/sqvm.h>
#include <../squirrel/sqclosure.h>

#include <map>
#include <memory>

#include "../safeguards.h"

/* Defined by GameInstance's translation unit (game_instance.cpp includes the generated
 * game_includes.hpp, whose functions have external linkage); including it here would define them twice. */
void SQGS_RegisterAll(Squirrel &engine);
void SQGSController_Register(Squirrel &engine);
void SQAI_RegisterAll(Squirrel &engine); // likewise from ai_instance.cpp
void SQAIController_Register(Squirrel &engine);

using json = nlohmann::json;

namespace PrismaticMCP::Api {

namespace {

/**
 * A script engine that only answers calls: one API ("GS" for Game Scripts, "AI" for company AIs) as
 * its instance registers it, and a controller that does nothing.
 */
class McpInstance : public ScriptInstance {
public:
	explicit McpInstance(std::string_view api) : ScriptInstance(api), api(api) {}

	void Start()
	{
		if (this->api == "AI") SQAIController_Register(*this->engine); else SQGSController_Register(*this->engine);
		ScriptInstance::Initialize("%_dummy", "PrismaticMcpApi", OWNER_DEITY);
	}

	Squirrel &Engine() { return *this->engine; }
	const std::string &Api() const { return this->api; }

	int GetSetting(const std::string &) override { return 0; }
	ScriptInfo *FindLibrary(const std::string &, int) override { return nullptr; }
	CommandCallbackData *GetDoCommandCallback() override { return nullptr; }

	void LoadDummyScript() override
	{
		HSQUIRRELVM vm = this->engine->GetVM();
		std::string main = fmt::format("class PrismaticMcpApi extends {}Controller {{ function Start() {{}} }}", this->api);
		sq_pushroottable(vm);
		if (SQ_SUCCEEDED(sq_compilebuffer(vm, main, "prismatic_mcp_api", SQTrue))) {
			sq_push(vm, -2);
			if (SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
				sq_pop(vm, 2);
				return;
			}
		}
		NOT_REACHED();
	}

protected:
	void RegisterAPI() override
	{
		ScriptInstance::RegisterAPI();
		if (this->api == "AI") SQAI_RegisterAll(*this->engine); else SQGS_RegisterAll(*this->engine);
	}

private:
	std::string api;
};

/** One static function of the API as the engine registered it. */
struct Function {
	std::string api;           ///< The engine that has it: GS, or AI for a function only company AIs get.
	std::string gs_class;      ///< The class's name in that engine, GS<Name> or AI<Name>.
	std::string name;          ///< The function's name in the engine.
	std::vector<SQInteger> types; ///< The engine's parameter checks after `this`: a mask of _RT_* bits, -1 for any.
	bool variadic = false;     ///< An advanced function: it reads its own arguments, so none are checked.
	bool global = false;       ///< Registered in the global scope rather than in a class (`import`).
};

/** ScriptObject's protected statics, for this surface only: the active instance, the error and cost
 * of the last call, and the command modes a call runs in. */
class Access : public ScriptObject {
public:
	using ScriptObject::ActiveInstance;
	static void ClearLastError() { ScriptObject::SetLastError(ScriptError::ERR_NONE); }
	static Money LastCost() { return ScriptObject::GetLastCost(); }
	static void SetCompany(::CompanyID c) { ScriptObject::SetCompany(c); }
	static ::CompanyID Company() { return ScriptObject::GetCompany(); }

	static bool Estimate() { return false; } ///< GSTestMode's answer: only test the command.
	static bool Asynchronous() { return true; } ///< GSAsyncMode's answer: post without waiting.

	/** One call's command mode: test (estimate only) or asynchronous execution, restored after it. */
	class Mode {
	public:
		explicit Mode(bool execute) :
			mode(ScriptObject::GetDoCommandMode()), mode_instance(ScriptObject::GetDoCommandModeInstance()),
			async(ScriptObject::GetDoCommandAsyncMode()), async_instance(ScriptObject::GetDoCommandAsyncModeInstance())
		{
			if (execute) {
				ScriptObject::SetDoCommandMode(nullptr, nullptr);
				ScriptObject::SetDoCommandAsyncMode(&Access::Asynchronous, nullptr);
			} else {
				ScriptObject::SetDoCommandMode(&Access::Estimate, nullptr);
			}
		}
		~Mode()
		{
			ScriptObject::SetDoCommandMode(this->mode, this->mode_instance);
			ScriptObject::SetDoCommandAsyncMode(this->async, this->async_instance);
		}
	private:
		ScriptModeProc *mode;
		ScriptObject *mode_instance;
		ScriptAsyncModeProc *async;
		ScriptObject *async_instance;
	};
};

std::unique_ptr<McpInstance> _gs; ///< The Game Script API: every function a game script gets.
std::unique_ptr<McpInstance> _ai; ///< The AI API: for the functions only company AIs get.
bool _started = false;
std::map<std::string, Function> _functions; ///< By tool name.

/**
 * The tool name of a function: openttd.api.Script<Name>.<Function>, the C++ class and function. The
 * engine's names are the C++ ones with the API prefix, except `import`, which the controller
 * registers in lower case; a name is given its C++ capital.
 */
std::string ToolName(const std::string &cls, const std::string &function)
{
	std::string fn = function;
	if (!fn.empty()) fn[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(fn[0])));
	return fmt::format("openttd.api.Script{}.{}", cls.substr(2), fn);
}

/** Read one engine's catalogue: its classes' native static functions, the ones not already known. */
void ReadCatalogue(McpInstance &inst)
{
	const std::string &api = inst.Api();
	Access::ActiveInstance active(inst);
	HSQUIRRELVM vm = inst.Engine().GetVM();
	SQInteger top = sq_gettop(vm);
	sq_pushroottable(vm);
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		std::string_view cls;
		if (sq_gettype(vm, -1) == OT_CLASS && SQ_SUCCEEDED(sq_getstring(vm, -2, cls)) && cls.starts_with(api) && cls != "PrismaticMcpApi") {
			std::string name{cls};
			SQInteger cls_idx = sq_gettop(vm);
			/* A derived class holds its base's functions too: each is listed once, under the class that
			 * declares it, which is the one whose base does not hold the same closure. */
			sq_getbase(vm, cls_idx);
			SQInteger base_idx = sq_gettop(vm);
			bool has_base = sq_gettype(vm, base_idx) == OT_CLASS;
			auto inherited = [&](std::string_view fn, const HSQOBJECT &o) {
				if (!has_base) return false;
				sq_pushstring(vm, fn);
				if (SQ_FAILED(sq_rawget(vm, base_idx))) return false;
				HSQOBJECT b;
				sq_getstackobj(vm, -1, &b);
				bool same = sq_type(b) == sq_type(o) && b._unVal.pNativeClosure == o._unVal.pNativeClosure;
				sq_pop(vm, 1);
				return same;
			};
			/* An advanced function has no parameter check, so it cannot tell a static function from a
			 * method: it is static in a class that has no constructor of its own and no checked method. */
			bool has_methods = false;
			sq_pushstring(vm, "constructor");
			if (SQ_SUCCEEDED(sq_rawget(vm, cls_idx))) {
				HSQOBJECT c;
				sq_getstackobj(vm, -1, &c);
				sq_pop(vm, 1);
				if (!inherited("constructor", c)) has_methods = true;
			}
			sq_pushnull(vm);
			while (SQ_SUCCEEDED(sq_next(vm, cls_idx))) {
				if (sq_gettype(vm, -1) == OT_NATIVECLOSURE) {
					HSQOBJECT o;
					sq_getstackobj(vm, -1, &o);
					const SQNativeClosure *nc = _nativeclosure(o);
					std::string_view fn;
					sq_getstring(vm, -2, fn);
					if (fn != "constructor" && nc->_typecheck.size() > 0 && nc->_typecheck[0] != -1) has_methods = true;
				}
				sq_pop(vm, 2);
			}
			sq_pop(vm, 1);
			sq_pushnull(vm);
			while (SQ_SUCCEEDED(sq_next(vm, cls_idx))) {
				std::string_view fn;
				if (sq_gettype(vm, -1) == OT_NATIVECLOSURE && SQ_SUCCEEDED(sq_getstring(vm, -2, fn)) && !fn.empty() && fn.find('@') == std::string_view::npos && fn != "constructor") {
					HSQOBJECT o;
					sq_getstackobj(vm, -1, &o);
					const SQNativeClosure *nc = _nativeclosure(o);
					/* A static function's first check is '.', any `this`; a method's is 'x', an instance. */
					bool checked_static = nc->_typecheck.size() > 0 && nc->_typecheck[0] == -1;
					bool advanced_static = nc->_typecheck.size() == 0 && !has_methods;
					std::string tool = ToolName(name, std::string{fn});
					if ((checked_static || advanced_static) && !_functions.contains(tool) && !inherited(fn, o)) {
						Function f{api, name, std::string{fn}, {}, advanced_static};
						for (SQUnsignedInteger i = 1; i < nc->_typecheck.size(); i++) f.types.push_back(nc->_typecheck[i]);
						_functions[tool] = std::move(f);
					}
				}
				sq_pop(vm, 2);
			}
			sq_pop(vm, 1); // the iterator
			sq_pop(vm, 1); // the base
		}
		/* The controller registers `import` in the global scope, not in its class (game_controller.sq.hpp). */
		std::string_view root_fn;
		if (api == "GS" && sq_gettype(vm, -1) == OT_NATIVECLOSURE && SQ_SUCCEEDED(sq_getstring(vm, -2, root_fn)) && root_fn == "import") {
			HSQOBJECT o;
			sq_getstackobj(vm, -1, &o);
			const SQNativeClosure *nc = _nativeclosure(o);
			Function f{api, "GSController", "import", {}, false};
			f.global = true;
			for (SQUnsignedInteger i = 1; i < nc->_typecheck.size(); i++) f.types.push_back(nc->_typecheck[i]);
			_functions[ToolName("GSController", "import")] = std::move(f);
		}
		sq_pop(vm, 2);
	}
	sq_settop(vm, top);
}

/** Start both engines and read their catalogues, once: the Game Script API first, then what only AIs get. */
void Start()
{
	if (_started) return;
	_started = true;
	/* A controller sets the current company; the game's own must survive the start. */
	AutoRestoreBackup cur_company(_current_company, _current_company);
	for (auto *slot : {&_gs, &_ai}) {
		*slot = std::make_unique<McpInstance>(slot == &_gs ? "GS" : "AI");
		(*slot)->Start();
		if ((*slot)->IsDead()) { slot->reset(); continue; }
		ReadCatalogue(**slot);
	}
}

McpInstance *Instance(const std::string &api)
{
	Start();
	McpInstance *inst = (api == "AI" ? _ai : _gs).get();
	return (inst == nullptr || inst->IsDead()) ? nullptr : inst;
}

/** The JSON Schema of one parameter from its check mask. */
json ParamSchema(SQInteger mask)
{
	if (mask == -1) return json{{"description", "any: a number, a boolean, or a string (a text parameter takes a plain string)"}};
	json types = json::array();
	if (mask & _RT_INTEGER) types.push_back("integer");
	if (mask & _RT_FLOAT) types.push_back("number");
	if (mask & _RT_BOOL) types.push_back("boolean");
	if (mask & _RT_STRING) types.push_back("string");
	if (mask & _RT_NULL) types.push_back("null");
	if (mask & (_RT_INSTANCE | _RT_ARRAY | _RT_TABLE)) return json{{"description", "a script object (GSList, GSText): not convertible from JSON yet"}};
	if (types.size() == 1) return json{{"type", types[0]}};
	return json{{"type", types}};
}

/** Push one JSON argument as the engine's value; false when JSON has no such value. */
bool Push(HSQUIRRELVM vm, const json &v)
{
	if (v.is_boolean()) { sq_pushbool(vm, v.get<bool>()); return true; }
	if (v.is_number_integer()) { sq_pushinteger(vm, v.get<int64_t>()); return true; }
	if (v.is_number_float()) { sq_pushfloat(vm, v.get<double>()); return true; }
	if (v.is_string()) { sq_pushstring(vm, v.get<std::string>()); return true; }
	if (v.is_null()) { sq_pushnull(vm); return true; }
	if (v.is_array()) {
		sq_newarray(vm, 0);
		for (const json &e : v) { if (!Push(vm, e)) return false; sq_arrayappend(vm, -2); }
		return true;
	}
	if (v.is_object()) {
		sq_newtable(vm);
		for (const auto &[k, e] : v.items()) { sq_pushstring(vm, k); if (!Push(vm, e)) return false; sq_newslot(vm, -3, SQFalse); }
		return true;
	}
	return false;
}

/** The value at the top of the stack as JSON: a GSList as its items, an array as an array. */
json Read(HSQUIRRELVM vm, SQInteger idx)
{
	switch (sq_gettype(vm, idx)) {
		case OT_NULL: return nullptr;
		case OT_BOOL: { SQBool b; sq_getbool(vm, idx, &b); return b != 0; }
		case OT_INTEGER: { SQInteger i; sq_getinteger(vm, idx, &i); return (int64_t)i; }
		case OT_FLOAT: { SQFloat f; sq_getfloat(vm, idx, &f); return (double)f; }
		case OT_STRING: { std::string_view s; sq_getstring(vm, idx, s); return std::string{s}; }
		case OT_ARRAY: {
			json out = json::array();
			SQInteger abs = idx < 0 ? sq_gettop(vm) + idx + 1 : idx;
			sq_pushnull(vm);
			while (SQ_SUCCEEDED(sq_next(vm, abs))) { out.push_back(Read(vm, -1)); sq_pop(vm, 2); }
			sq_pop(vm, 1);
			return out;
		}
		case OT_INSTANCE: {
			SQUserPointer up = nullptr;
			if (SQ_SUCCEEDED(sq_getinstanceup(vm, idx, &up, nullptr)) && up != nullptr) {
				if (auto *list = dynamic_cast<ScriptList *>(static_cast<ScriptObject *>(up)); list != nullptr) {
					json items = json::array();
					for (SQInteger item = list->Begin(); !list->IsEnd(); item = list->Next()) {
						items.push_back(json{{"item", (int64_t)item}, {"value", (int64_t)list->GetValue(item)}});
					}
					return json{{"list", items}, {"count", list->Count()}};
				}
			}
			return json{{"instance", "a script object this surface does not convert"}};
		}
		default: return json{{"unsupported_type", (int)sq_gettype(vm, idx)}};
	}
}

/** Run one function: mode test (estimate) or async (execute). */
json Run(const Function &f, const json &args, bool execute)
{
	McpInstance *inst = Instance(f.api);
	if (inst == nullptr) return json{{"ok", false}, {"error", "the private " + f.api + " script engine did not start"}};
	json pos = args.value("args", json::array());
	if (!pos.is_array()) return json{{"ok", false}, {"error", "args must be an array of positional arguments"}};
	if (!f.variadic && pos.size() != f.types.size()) {
		return json{{"ok", false}, {"error", fmt::format("Script{}.{} takes {} argument(s), got {}", f.gs_class.substr(2), f.name, f.types.size(), pos.size())}};
	}

	/* A call acts as the deity or the company it names, and the game's current company is put back. */
	AutoRestoreBackup cur_company(_current_company, OWNER_DEITY);
	Access::ActiveInstance active(*inst);
	HSQUIRRELVM vm = inst->Engine().GetVM();
	SQInteger top = sq_gettop(vm);
	json out = {{"function", fmt::format("Script{}.{}", f.gs_class.substr(2), f.name)}, {"mode", execute ? "execute" : "test"}};
	try {
		std::optional<ScriptCompanyMode> company;
		if (args.contains("company") && args["company"].is_number_integer()) {
			company.emplace(static_cast<ScriptCompany::CompanyID>(args["company"].get<int>()));
			out["company"] = args["company"];
		}
		Access::Mode mode(execute);
		Access::ClearLastError();

		sq_pushroottable(vm);
		if (!f.global) {
			sq_pushstring(vm, f.gs_class);
			if (SQ_FAILED(sq_get(vm, -2))) { sq_settop(vm, top); return json{{"ok", false}, {"error", "class missing from the engine"}}; }
		}
		sq_pushstring(vm, f.name);
		if (SQ_FAILED(sq_get(vm, -2))) { sq_settop(vm, top); return json{{"ok", false}, {"error", "function missing from the engine"}}; }
		sq_push(vm, -2); // the class as `this`
		for (const json &a : pos) {
			if (!Push(vm, a)) { sq_settop(vm, top); return json{{"ok", false}, {"error", "an argument has no script value"}}; }
		}
		if (SQ_FAILED(sq_call(vm, (SQInteger)pos.size() + 1, SQTrue, SQFalse))) {
			sq_getlasterror(vm);
			json err = Read(vm, -1);
			sq_settop(vm, top);
			return json{{"ok", false}, {"error", err}, {"function", out["function"]}};
		}
		out["result"] = Read(vm, -1);
		/* The script's own last error: null when the call left none (ERR_NONE). */
		std::string last = ScriptError::GetLastErrorString().value_or("");
		out["last_error"] = (last.empty() || last == "ERR_NONE") ? json(nullptr) : json(last);
		out["cost"] = (int64_t)Access::LastCost();
		out["ok"] = true;
	} catch (Script_FatalError &e) {
		out["ok"] = false;
		out["error"] = e.GetErrorMessage();
	} catch (Script_Suspend &) {
		out["ok"] = true;
		out["note"] = "the command was posted; the script engine asked to wait for it";
	}
	sq_settop(vm, top);
	return out;
}

const Function *Find(const std::string &tool)
{
	if (!tool.starts_with("openttd.api.")) return nullptr;
	Start();
	auto it = _functions.find(tool);
	return it == _functions.end() ? nullptr : &it->second;
}

} // namespace

json Tools()
{
	json arr = json::array();
	Start();
	for (const auto &[tool, f] : _functions) {
		json items = json::array();
		for (SQInteger m : f.types) items.push_back(ParamSchema(m));
		json args = {{"type", "array"}, {"description", "the function's arguments in order, as " + f.gs_class + "." + f.name + " takes them"}};
		if (f.variadic) {
			args["description"] = "the function's arguments in order, as " + f.gs_class + "." + f.name + " reads them itself (an advanced function: the engine checks none)";
		} else {
			args["prefixItems"] = items;
			args["minItems"] = items.size();
			args["maxItems"] = items.size();
		}
		json props = {
			{"args", args},
			{"company", {{"type", "integer"}, {"description", "act as this company (GSCompanyMode); default the deity"}}},
			{"execute", {{"type", "boolean"}, {"description", "issue the command it estimates; approval-gated. Default false: GSTestMode, nothing changes"}}},
		};
		arr.push_back({{"name", tool},
			{"description", fmt::format("Script API: Script{}.{} ({}.{} in {}), run through the game's own binding. Without execute it runs in test mode and changes nothing", f.gs_class.substr(2), f.name, f.gs_class, f.name, f.api == "AI" ? "a company AI; only AIs get it" : "a game script")},
			{"inputSchema", {{"type", "object"}, {"properties", props}, {"required", (items.empty() && !f.variadic) ? json::array() : json::array({"args"})}}},
			{"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", false}}}});
	}
	return arr;
}

bool WantsExecution(const std::string &name, const json &args)
{
	return name.starts_with("openttd.api.") && args.value("execute", false);
}

std::optional<json> Call(const std::string &name, const json &args)
{
	if (!name.starts_with("openttd.api.")) return std::nullopt;
	const Function *f = Find(name);
	if (f == nullptr) return json{{"ok", false}, {"error", "no such Script API function: " + name}};
	return Run(*f, args, false);
}

json Validate(const std::string &type, const json &args, json &errors)
{
	const Function *f = Find("openttd." + type);
	if (f == nullptr) { errors.push_back("no such Script API function: " + type); return json::object(); }
	json est = Run(*f, args, false);
	if (!est.value("ok", false)) errors.push_back(est.value("error", json("the estimate failed")).dump());
	return json{{"estimate", est}};
}

json Execute(const std::string &type, const json &args)
{
	const Function *f = Find("openttd." + type);
	if (f == nullptr) return json{{"ok", false}, {"error", "no such Script API function: " + type}};
	return Run(*f, args, true);
}

} // namespace PrismaticMCP::Api
