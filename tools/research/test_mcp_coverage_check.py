#!/usr/bin/env python3
"""Tests for the argument derivation of mcp-coverage-check, offline (no server, no game).

Run: python3 tools/research/test_mcp_coverage_check.py
(or python3 -B -m unittest tools/research/test_mcp_coverage_check.py; -B keeps __pycache__ out of
the work tree, which ignores none here).

The fixture is the shape of every inputSchema the C++ server publishes, copied from
src/prismatic_mcp (each entry names its source); the values that are build-dependent (an action
type list, a command's parameter list) are a representative selection of what the same code emits.
Each derived value is checked by a small validator of the JSON Schema keywords the fixture uses;
it refuses a keyword it does not know, so a new keyword in the fixture cannot pass unchecked.
"""
import importlib.machinery
import importlib.util
import json
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
PROBE_PATH = HERE / "mcp-coverage-check"
_loader = importlib.machinery.SourceFileLoader("mcp_coverage_check", str(PROBE_PATH))
_spec = importlib.util.spec_from_loader("mcp_coverage_check", _loader)
probe = importlib.util.module_from_spec(_spec)
_loader.exec_module(probe)


# ---------------------------------------------------------------- a validator for the fixture

ANNOTATIONS = {"description", "title", "default", "x-kind", "x-cpp-type"}
CHECKED = {"type", "enum", "const", "minimum", "maximum", "exclusiveMinimum", "exclusiveMaximum",
           "multipleOf", "minLength", "maxLength", "minItems", "maxItems", "prefixItems", "items",
           "properties", "required", "additionalProperties", "anyOf", "oneOf", "allOf"}
TYPES = {
    "integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
    "number": lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
    "string": lambda v: isinstance(v, str),
    "boolean": lambda v: isinstance(v, bool),
    "null": lambda v: v is None,
    "array": lambda v: isinstance(v, list),
    "object": lambda v: isinstance(v, dict),
}


def same(a, b):
    """JSON equality (Python's 0 == False is not JSON's)."""
    return json.dumps(a, sort_keys=True) == json.dumps(b, sort_keys=True)


def errors(value, schema, path="$"):
    """Every way the value fails the schema, as readable strings; [] when it validates."""
    out = []
    unknown = set(schema) - CHECKED - ANNOTATIONS
    if unknown:
        return [f"{path}: the test validator does not check {sorted(unknown)}"]
    if "type" in schema:
        kinds = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        if not any(TYPES[k](value) for k in kinds):
            out.append(f"{path}: {value!r} is not {kinds}")
            return out
    if "enum" in schema and not any(same(value, e) for e in schema["enum"]):
        out.append(f"{path}: {value!r} not in enum {schema['enum']}")
    if "const" in schema and not same(value, schema["const"]):
        out.append(f"{path}: {value!r} is not const {schema['const']!r}")
    if TYPES["number"](value):
        if "minimum" in schema and value < schema["minimum"]:
            out.append(f"{path}: {value} < minimum {schema['minimum']}")
        if "maximum" in schema and value > schema["maximum"]:
            out.append(f"{path}: {value} > maximum {schema['maximum']}")
        if "exclusiveMinimum" in schema and value <= schema["exclusiveMinimum"]:
            out.append(f"{path}: {value} <= exclusiveMinimum {schema['exclusiveMinimum']}")
        if "exclusiveMaximum" in schema and value >= schema["exclusiveMaximum"]:
            out.append(f"{path}: {value} >= exclusiveMaximum {schema['exclusiveMaximum']}")
        if "multipleOf" in schema and (value / schema["multipleOf"]) % 1:
            out.append(f"{path}: {value} is not a multiple of {schema['multipleOf']}")
    if isinstance(value, str):
        if len(value) < schema.get("minLength", 0):
            out.append(f"{path}: shorter than minLength {schema['minLength']}")
        if "maxLength" in schema and len(value) > schema["maxLength"]:
            out.append(f"{path}: longer than maxLength {schema['maxLength']}")
    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0):
            out.append(f"{path}: {len(value)} items < minItems {schema['minItems']}")
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            out.append(f"{path}: {len(value)} items > maxItems {schema['maxItems']}")
        prefix = schema.get("prefixItems", [])
        for i, item in enumerate(value):
            sub = prefix[i] if i < len(prefix) else schema.get("items", {})
            out += errors(item, sub, f"{path}[{i}]") if isinstance(sub, dict) else (
                [] if sub is not False else [f"{path}[{i}]: no item allowed here"])
    if isinstance(value, dict):
        props = schema.get("properties", {})
        for name in schema.get("required", []):
            if name not in value:
                out.append(f"{path}: missing required {name!r}")
        for name, item in value.items():
            if name in props:
                out += errors(item, props[name], f"{path}.{name}")
            elif schema.get("additionalProperties") is False:
                out.append(f"{path}: {name!r} is not allowed")
            elif isinstance(schema.get("additionalProperties"), dict):
                out += errors(item, schema["additionalProperties"], f"{path}.{name}")
    for sub in schema.get("allOf", []):
        out += errors(value, sub, path)
    if "anyOf" in schema and not any(not errors(value, s, path) for s in schema["anyOf"]):
        out.append(f"{path}: {value!r} matches no anyOf branch")
    if "oneOf" in schema:
        hits = sum(1 for s in schema["oneOf"] if not errors(value, s, path))
        if hits != 1:
            out.append(f"{path}: {value!r} matches {hits} oneOf branches, not one")
    return out


# ---------------------------------------------------------------- the fixture, from the C++ source

def int_schema(lo, hi, cpp=None, kind=None):
    """params.hpp IntSchema<B>(), with what Schema<T>() adds."""
    s = {"type": "integer", "minimum": lo, "maximum": hi}
    if kind:
        s["x-kind"] = kind
    if cpp:
        s["x-cpp-type"] = cpp
    return s


U8, U16, U32 = (0, 255), (0, 65535), (0, 4294967295)
I8, I64 = (-128, 127), (-(2 ** 63), 2 ** 63 - 1)
U64 = (0, 2 ** 64 - 1)

# params.hpp Schema<T>(): one entry per branch of the if-constexpr chain.
P_TEXT = {"type": "string", "maxLength": 1024, "x-kind": "text", "x-cpp-type": "EncodedString",
          "description": "plain text, shown as written (invalid characters become '?')"}
P_VEHICLE_LIST = {
    "type": "object", "x-kind": "vehicle_list", "required": ["type", "vtype", "company", "index"],
    "properties": {
        "type": {"type": "integer", "minimum": 0, "maximum": 4,
                 "description": "0 company, 1 shared orders, 2 station, 3 depot, 4 group"},
        "vtype": {"type": "integer", "minimum": 0, "maximum": 3,
                  "description": "0 train, 1 road, 2 ship, 3 aircraft"},
        "company": int_schema(*U8), "index": int_schema(*U32)},
    "additionalProperties": False, "x-cpp-type": "VehicleListIdentifier"}
P_ORDER = {
    "type": "object", "x-kind": "order", "required": ["type", "flags", "dest"],
    "description": "the order's wire fields, as OpenTTD sends an order to the server",
    "properties": {
        "type": {"type": "integer", "minimum": 0, "maximum": 255},
        "flags": int_schema(*U8), "dest": int_schema(*U16), "refit_cargo": int_schema(*U8),
        "wait_time": int_schema(*U16), "travel_time": int_schema(*U16),
        "max_speed": int_schema(*U16)},
    "additionalProperties": False, "x-cpp-type": "Order"}
P_SOURCE = {
    "type": "object", "x-kind": "source", "required": ["id", "type"],
    "properties": {"id": int_schema(*U16), "type": {"type": "integer", "minimum": 0, "maximum": 2}},
    "additionalProperties": False, "x-cpp-type": "Source"}


def variant(*alternatives):
    """params.hpp Schema<std::variant<...>>: null for std::monostate, else {index, value}."""
    options = []
    for i, alt in enumerate(alternatives):
        if alt is None:
            options.append({"type": "null", "description": "none"})
        else:
            options.append({"type": "object", "required": ["index", "value"],
                            "additionalProperties": False,
                            "properties": {"index": {"const": i}, "value": alt}})
    return {"oneOf": options, "x-kind": "variant", "x-cpp-type": "std::variant<...>"}


P_VARIANT_EMPTY = variant(None, int_schema(*U16, cpp="TileIndex"), {"type": "boolean"})
P_VARIANT_FULL = variant(int_schema(*U16, cpp="StationID", kind="id"), P_TEXT)
P_BOOL = {"type": "boolean", "x-cpp-type": "bool"}
P_STRING = {"type": "string", "x-cpp-type": "std::string"}
P_ENUM = int_schema(*U8, cpp="RailType", kind="enum")
P_ID = int_schema(*U16, cpp="TownID", kind="id")
P_TILE = dict(int_schema(*U32, cpp="TileIndex"), description="tile index: y * map_width + x")
P_SIGNED = int_schema(*I8, cpp="int8_t")
P_U64 = int_schema(*U64, cpp="uint64_t")
P_MONEY = int_schema(*I64, cpp="Money", kind="money")
COMMAND_PARAMS = [P_TILE, P_ENUM, P_ID, P_BOOL, P_STRING, P_TEXT, P_SIGNED, P_U64, P_MONEY,
                  P_VEHICLE_LIST, P_ORDER, P_SOURCE, P_VARIANT_EMPTY, P_VARIANT_FULL]


def command_schema(params, location):
    """commands.cpp ToolInputSchema(Commands)."""
    props = {
        "company": {"description": "Who the command is posted as",
                    "anyOf": [{"type": "integer", "minimum": 0, "maximum": 254},
                              {"type": "string", "enum": ["server", "deity"]}]},
        "params": {"description": "The command's parameters, positionally", "type": "array",
                   "prefixItems": params, "minItems": len(params), "maxItems": len(params)}}
    required = ["params"]
    if location:
        props["location"] = {"type": "integer", "minimum": 0, "description": "tile index"}
        required.append("location")
    return {"type": "object", "properties": props, "required": required}


def param_schema(*types):
    """api.cpp ParamSchema(mask): one type, a union, or a description only."""
    if not types:
        return {"description": "any: a number, a boolean, or a string"}
    return {"type": types[0]} if len(types) == 1 else {"type": list(types)}


def api_schema(items, variadic=False):
    """api.cpp Tools(): a Script API function."""
    args = {"type": "array", "description": "the function's arguments in order"}
    if not variadic:
        args.update({"prefixItems": items, "minItems": len(items), "maxItems": len(items)})
    props = {"args": args,
             "company": {"type": "integer", "description": "act as this company"},
             "execute": {"type": "boolean", "description": "issue the command it estimates"}}
    required = [] if (not items and not variadic) else ["args"]
    return {"type": "object", "properties": props, "required": required}


ACTION_TYPES = ["type_a", "type_b"]  # server.cpp ActionTypes(): build-dependent strings
ACTION = {"type": "object", "required": ["type"],
          "properties": {"type": {"type": "string", "enum": ACTION_TYPES},
                         "arguments": {"type": "object"}}}
COMPANY = {"type": "integer", "minimum": 0, "maximum": 14}  # ai.cpp: MAX_COMPANIES - 1
TILE = {"tile": {"type": "integer"}, "x": {"type": "integer"}, "y": {"type": "integer"}}
MODS = {"type": "array", "items": {"type": "string", "enum": ["ctrl", "shift"]}}
WINDOW = {"type": "object", "description": "the front-most window matching any of",
          "properties": {"class": {"type": "integer"}, "number": {"type": "integer"},
                         "caption": {"type": "string"}}}


def ui_schema(properties, required=()):
    """ui.cpp Schema(properties, required)."""
    return {"type": "object", "properties": properties, "required": list(required)}


FIXTURE = {
    "commands.cpp ToolInputSchema, with location": command_schema(COMMAND_PARAMS, True),
    "commands.cpp ToolInputSchema, no location": command_schema([P_ID, P_BOOL], False),
    "commands.cpp ToolInputSchema, no parameters": command_schema([], False),
    "api.cpp Tools, fixed arity": api_schema([
        param_schema("integer"), param_schema("integer", "number"), param_schema("boolean"),
        param_schema("string"), param_schema("integer", "null"), param_schema(),
        {"description": "a script object (GSList, GSText): not convertible from JSON yet"}]),
    "api.cpp Tools, variadic": api_schema([], variadic=True),
    "api.cpp Tools, no arguments": api_schema([]),
    "server.cpp decision validate": {"type": "object", "properties": {
        "action": ACTION, "action_type": {"type": "string", "enum": ACTION_TYPES},
        "arguments": {"type": "object"}}},
    "server.cpp decision list_action_types": {"type": "object"},
    "server.cpp decision submit": {"type": "object", "required": ["action"],
                                   "properties": {"action": ACTION}},
    "server.cpp game set_pause": {"type": "object", "properties": {"pause": {"type": "boolean"}},
                                  "required": ["pause"]},
    "server.cpp decision get": {"type": "object", "required": ["decision_id"], "properties": {
        "decision_id": {"type": "integer", "minimum": 1}}},
    "ai.cpp set_setting": {"type": "object", "required": ["company", "name", "value"],
                           "properties": {"company": COMPANY, "name": {"type": "string"},
                                          "value": {"type": "integer"}}},
    "ai.cpp start": {"type": "object", "properties": {
        "name": {"type": "string"}, "version": {"type": "integer"},
        "settings": {"type": "object", "additionalProperties": {"type": "integer"}}}},
    "ai.cpp stop": {"type": "object", "properties": {"company": COMPANY},
                    "required": ["company"]},
    "view.cpp news.show": {"type": "object", "required": ["text"], "properties": {
        "text": {"type": "string"},
        "style": {"type": "string", "enum": ["normal", "thin", "small", "company"]},
        "company": {"type": "integer"},
        "title": {"type": "string", "enum": ["none", "launch", "trouble", "merger", "bankrupt"]},
        "tile": {"type": "integer"}}},
    "view.cpp ask.auto": {"type": "object", "properties": {
        "answer": {"type": ["string", "null"]},
        "countdown": {"type": "integer", "minimum": 0, "maximum": 600}}},
    "view.cpp console.print": {"type": "object", "required": ["text"], "properties": {
        "text": {"type": "string"},
        "level": {"type": "string", "enum": ["info", "warning", "error", "debug", "rhai", "plan",
                                             "plain"]}}},
    "ui.cpp at": ui_schema({"x": {"type": "integer"}, "y": {"type": "integer"}}, ["x", "y"]),
    "ui.cpp locate": {"type": "object", "properties": TILE,
                      "anyOf": [{"required": ["tile"]}, {"required": ["x", "y"]}]},
    "ui.cpp button": ui_schema({"state": {"type": "string", "enum": ["down", "up"]},
                                "button": {"type": "string", "enum": ["left", "right", "middle"]},
                                "mods": MODS}, ["state"]),
    "ui.cpp press": ui_schema({"window": WINDOW, "widget": {"type": "object", "properties": {
        "index": {"type": "integer"}, "text": {"type": "string"}}}, "mods": MODS,
        "hold": {"type": "boolean"}}, ["window", "widget"]),
    "ui.cpp choose": {"type": "object", "properties": {
        "index": {"type": "integer"}, "result": {"type": "integer"}, "text": {"type": "string"}},
        "anyOf": [{"required": ["index"]}, {"required": ["result"]}, {"required": ["text"]}]},
    "menu.cpp requests.answer": {"type": "object", "required": ["id", "answer"], "properties": {
        "id": {"type": "integer"}, "answer": {"type": "string"}}},
}

# Keywords the probe derives that no published schema uses today (checked against hand values).
SYNTHETIC = {
    "exclusiveMinimum": {"type": "integer", "exclusiveMinimum": 0},
    "exclusiveMaximum": {"type": "number", "exclusiveMaximum": -2},
    "bounded below zero": {"type": "integer", "minimum": -10, "maximum": -3},
    "multipleOf": {"type": "integer", "minimum": 1, "multipleOf": 5},
    "minLength": {"type": "string", "minLength": 3, "maxLength": 8},
    "default": {"type": "integer", "default": 7, "minimum": 0},
    "default outside enum": {"type": "string", "default": "x", "enum": ["a", "b"]},
    "const": {"const": "fixed"},
    "allOf": {"allOf": [{"type": "object", "required": ["a"]},
                        {"properties": {"a": {"type": "integer", "minimum": 2}}}]},
    "minItems with items": {"type": "array", "minItems": 2, "items": {"type": "boolean"}},
    "prefixItems then items": {"type": "array", "prefixItems": [{"type": "string"}],
                               "minItems": 3, "items": {"type": "integer", "minimum": 4}},
    "untyped object": {"required": ["k"], "properties": {"k": {"enum": [3, 4]}}},
    "nested required": {"type": "object", "required": ["outer"], "properties": {"outer": {
        "type": "object", "required": ["inner"], "properties": {"inner": {
            "type": "array", "minItems": 1, "items": {"type": "integer", "minimum": 9}}}}}},
}


def subschemas(schema, path="$"):
    """Every schema nested in a schema, with where it sits."""
    yield path, schema
    for name, sub in schema.get("properties", {}).items():
        yield from subschemas(sub, f"{path}.{name}")
    for i, sub in enumerate(schema.get("prefixItems", [])):
        yield from subschemas(sub, f"{path}[{i}]")
    if isinstance(schema.get("items"), dict):
        yield from subschemas(schema["items"], f"{path}[]")
    if isinstance(schema.get("additionalProperties"), dict):
        yield from subschemas(schema["additionalProperties"], f"{path}.*")
    for key in ("allOf", "anyOf", "oneOf"):
        for i, sub in enumerate(schema.get(key, [])):
            yield from subschemas(sub, f"{path}/{key}/{i}")


def naive_args(schema):
    """The derivation this change replaces, kept to show the validator rejects what it built."""
    args = {}
    props = (schema or {}).get("properties", {})
    for name in (schema or {}).get("required", []):
        kind = props.get(name, {}).get("type", "string")
        kind = kind[0] if isinstance(kind, list) else kind
        args[name] = {"integer": 0, "number": 0, "boolean": False, "array": [],
                      "object": {}}.get(kind, "")
    return args


class DerivedArgsValidate(unittest.TestCase):
    def test_every_published_shape(self):
        for label, schema in FIXTURE.items():
            with self.subTest(label):
                args = probe.derived_args(schema)
                self.assertIsInstance(args, dict)
                self.assertEqual(errors(args, schema), [], f"{label}: {args!r}")

    def test_every_nested_schema(self):
        for label, schema in FIXTURE.items():
            for path, sub in subschemas(schema):
                with self.subTest(f"{label} {path}"):
                    value = probe.derived_value(sub)
                    self.assertEqual(errors(value, sub), [], f"{label} {path}: {value!r}")

    def test_synthetic_keywords(self):
        for label, schema in SYNTHETIC.items():
            with self.subTest(label):
                value = probe.derived_value(schema)
                self.assertEqual(errors(value, schema), [], f"{label}: {value!r}")


class DerivedValues(unittest.TestCase):
    def test_command_params_fill_prefix_items(self):
        args = probe.derived_args(FIXTURE["commands.cpp ToolInputSchema, with location"])
        self.assertEqual(len(args["params"]), len(COMMAND_PARAMS))
        self.assertEqual(args["location"], 0)
        self.assertNotIn("company", args)  # optional: omitted, the server's default applies
        self.assertEqual(args["params"][9], {"type": 0, "vtype": 0, "company": 0, "index": 0})
        self.assertEqual(args["params"][10], {"type": 0, "flags": 0, "dest": 0})
        self.assertIsNone(args["params"][12])  # a variant with std::monostate: null
        self.assertEqual(args["params"][13], {"index": 0, "value": 0})

    def test_api_union_and_optional_execute(self):
        args = probe.derived_args(FIXTURE["api.cpp Tools, fixed arity"])
        self.assertEqual(args, {"args": [0, 0, False, "", 0, 0, 0]})
        self.assertEqual(probe.derived_args(FIXTURE["api.cpp Tools, variadic"]), {"args": []})
        self.assertEqual(probe.derived_args(FIXTURE["api.cpp Tools, no arguments"]), {})

    def test_minimum_enum_and_object_any_of(self):
        self.assertEqual(probe.derived_args(FIXTURE["server.cpp decision get"]),
                         {"decision_id": 1})
        self.assertEqual(probe.derived_args(FIXTURE["server.cpp decision submit"]),
                         {"action": {"type": ACTION_TYPES[0]}})
        self.assertEqual(probe.derived_args(FIXTURE["ui.cpp locate"]), {"tile": 0})
        self.assertEqual(probe.derived_args(FIXTURE["ui.cpp choose"]), {"index": 0})
        self.assertEqual(probe.derived_args(FIXTURE["ui.cpp button"]), {"state": "down"})
        self.assertEqual(probe.derived_value({"type": ["string", "null"]}), "")
        self.assertEqual(probe.derived_value({"type": ["null", "integer"]}), 0)

    def test_synthetic_values(self):
        d = probe.derived_value
        self.assertEqual(d(SYNTHETIC["exclusiveMinimum"]), 1)
        self.assertEqual(d(SYNTHETIC["bounded below zero"]), -3)
        self.assertEqual(d(SYNTHETIC["multipleOf"]), 5)
        self.assertEqual(d(SYNTHETIC["minLength"]), "aaa")
        self.assertEqual(d(SYNTHETIC["default"]), 7)
        self.assertEqual(d(SYNTHETIC["default outside enum"]), "a")
        self.assertEqual(d(SYNTHETIC["allOf"]), {"a": 2})
        self.assertEqual(d(SYNTHETIC["prefixItems then items"]), ["", 4, 4])
        self.assertEqual(d(SYNTHETIC["nested required"]), {"outer": {"inner": [9]}})
        # draft 4 spellings: a boolean exclusiveMinimum and a tuple `items`
        self.assertEqual(d({"type": "integer", "minimum": 0, "exclusiveMinimum": True}), 1)
        self.assertEqual(d({"type": "array", "items": [{"type": "boolean"}, {"enum": ["z"]}]}),
                         [False, "z"])

    def test_validator_rejects_the_naive_derivation(self):
        # The old derivation built these; the game refused them while coverage counted them.
        for label in ("commands.cpp ToolInputSchema, with location", "api.cpp Tools, fixed arity",
                      "server.cpp decision get", "server.cpp decision submit", "ui.cpp locate",
                      "ui.cpp button"):
            with self.subTest(label):
                schema = FIXTURE[label]
                self.assertNotEqual(errors(naive_args(schema), schema), [], label)

    def test_probe_names_no_tool(self):
        self.assertNotIn("openttd.", PROBE_PATH.read_text())


if __name__ == "__main__":
    unittest.main(verbosity=1)
