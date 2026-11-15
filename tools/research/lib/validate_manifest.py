#!/usr/bin/env python3
"""Validate an experiment manifest against tools/research/schema/experiment-manifest.schema.json.

Usage: validate_manifest.py <schema.json> <manifest.json>

Exit 0 when the manifest is valid, 1 when it is not (each violation printed with its JSON path),
2 on a usage or read error. Uses the `jsonschema` package when it is installed. Otherwise it
falls back to a standard-library validator for the subset of draft 2020-12 the manifest schema
uses (type, required, properties, const, enum, minLength, items, format as an annotation). That
fallback fails closed: a keyword it does not implement is an error, never silently ignored.
"""
import json
import sys

# Keywords the fallback validates, and keywords that carry no assertion.
ASSERTING = {"type", "required", "properties", "const", "enum", "minLength", "items"}
ANNOTATIONS = {"$schema", "$id", "title", "description", "format"}

TYPES = {
    "object": lambda v: isinstance(v, dict),
    "array": lambda v: isinstance(v, list),
    "string": lambda v: isinstance(v, str),
    "integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
    "number": lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
    "boolean": lambda v: isinstance(v, bool),
    "null": lambda v: v is None,
}


def fallback(schema, value, path, errors):
    """Validate value against schema, appending '<path>: <message>' to errors."""
    unknown = set(schema) - ASSERTING - ANNOTATIONS
    if unknown:
        errors.append(f"{path}: schema keyword(s) {sorted(unknown)} not implemented by the fallback validator")
        return
    if "type" in schema:
        types = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        if not any(TYPES[t](value) for t in types):
            errors.append(f"{path}: expected {'/'.join(types)}, got {type(value).__name__}")
            return
    if "const" in schema and value != schema["const"]:
        errors.append(f"{path}: expected {schema['const']!r}, got {value!r}")
    if "enum" in schema and value not in schema["enum"]:
        errors.append(f"{path}: {value!r} is not one of {schema['enum']}")
    if "minLength" in schema and isinstance(value, str) and len(value) < schema["minLength"]:
        errors.append(f"{path}: shorter than {schema['minLength']}")
    if isinstance(value, dict):
        for key in schema.get("required", []):
            if key not in value:
                errors.append(f"{path}: missing required property {key!r}")
        for key, sub in schema.get("properties", {}).items():
            if key in value:
                fallback(sub, value[key], f"{path}.{key}", errors)
    if isinstance(value, list) and "items" in schema:
        for i, item in enumerate(value):
            fallback(schema["items"], item, f"{path}[{i}]", errors)


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        return 2
    try:
        with open(argv[1], encoding="utf-8") as f:
            schema = json.load(f)
        with open(argv[2], encoding="utf-8") as f:
            manifest = json.load(f)
    except (OSError, ValueError) as e:
        print(f"schema validation: cannot read input: {e}", file=sys.stderr)
        return 2

    try:
        import jsonschema  # noqa: PLC0415 - optional dependency
    except ImportError:
        jsonschema = None

    if jsonschema is not None:
        validator = jsonschema.Draft202012Validator(schema)
        errors = [f"$.{'.'.join(str(p) for p in e.absolute_path)}: {e.message}"
                  for e in validator.iter_errors(manifest)]
        engine = f"jsonschema {jsonschema.__version__}"
    else:
        errors = []
        fallback(schema, manifest, "$", errors)
        engine = "built-in subset validator"

    if errors:
        for e in errors:
            print(f"schema validation ({engine}): {e}")
        return 1
    print(f"schema validation ({engine}): valid")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
