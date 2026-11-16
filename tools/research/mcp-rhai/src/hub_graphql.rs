//! RESEARCH-ONLY / UNSTABLE: GraphQL over the hub, derived from its method table.
//!
//! A subset, implemented here so the hub needs no new dependency: one operation per document
//! (`query` or `mutation`, optionally named), fields with aliases and arguments, variables
//! (`$name`, from the request's `variables`), and literal strings, numbers, booleans, null, lists
//! and objects. Every result is JSON; a selection set keeps only the named keys of each object
//! (and of each object in a list), so a client asks for exactly what it reads. Not supported, and
//! refused by name: fragments, directives, introspection (`__schema`), subscriptions (stream over
//! the WebSocket's `hub.subscribe` instead).
//!
//! The root fields are the hub's JSON-RPC methods: a method that only reads is a Query field, one
//! that acts (hub.call) is a Mutation field, named by its last segment (`hub.audit` -> `audit`,
//! `rpc.discover` -> `discover`). Arguments are the method's params. A method the hub gains is a
//! field with no change here.

use serde_json::{json, Map, Value};

/// A field of a selection set: its result key (the alias, else the name), name, arguments and
/// its own selection set (empty: the whole value).
#[derive(Debug, Clone, PartialEq)]
pub struct Field {
    pub key: String,
    pub name: String,
    pub args: Map<String, Value>,
    pub selection: Vec<Field>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Operation {
    pub kind: String,
    pub selection: Vec<Field>,
}

struct Parser<'a> {
    src: &'a [u8],
    pos: usize,
    vars: &'a Map<String, Value>,
}

impl<'a> Parser<'a> {
    fn skip(&mut self) {
        while self.pos < self.src.len() {
            match self.src[self.pos] {
                b' ' | b'\t' | b'\n' | b'\r' | b',' => self.pos += 1,
                b'#' => while self.pos < self.src.len() && self.src[self.pos] != b'\n' { self.pos += 1 },
                _ => break,
            }
        }
    }

    fn peek(&mut self) -> Option<u8> {
        self.skip();
        self.src.get(self.pos).copied()
    }

    fn expect(&mut self, c: u8) -> Result<(), String> {
        if self.peek() == Some(c) { self.pos += 1; Ok(()) } else {
            Err(format!("expected '{}' at {}", c as char, self.pos))
        }
    }

    fn name(&mut self) -> Result<String, String> {
        self.skip();
        let start = self.pos;
        while self.pos < self.src.len() && (self.src[self.pos].is_ascii_alphanumeric() || self.src[self.pos] == b'_') {
            self.pos += 1;
        }
        if start == self.pos { return Err(format!("expected a name at {}", self.pos)); }
        Ok(String::from_utf8_lossy(&self.src[start..self.pos]).into_owned())
    }

    fn value(&mut self) -> Result<Value, String> {
        match self.peek() {
            Some(b'$') => {
                self.pos += 1;
                let n = self.name()?;
                Ok(self.vars.get(&n).cloned().unwrap_or(Value::Null))
            }
            Some(b'"') => {
                let start = self.pos;
                self.pos += 1;
                while self.pos < self.src.len() && self.src[self.pos] != b'"' {
                    if self.src[self.pos] == b'\\' { self.pos += 1; }
                    self.pos += 1;
                }
                self.pos += 1;
                serde_json::from_slice(&self.src[start..self.pos.min(self.src.len())]).map_err(|e| format!("bad string: {e}"))
            }
            Some(b'[') => {
                self.pos += 1;
                let mut out = Vec::new();
                while self.peek() != Some(b']') {
                    if self.peek().is_none() { return Err("unclosed list".into()); }
                    out.push(self.value()?);
                }
                self.pos += 1;
                Ok(Value::Array(out))
            }
            Some(b'{') => {
                self.pos += 1;
                let mut out = Map::new();
                while self.peek() != Some(b'}') {
                    if self.peek().is_none() { return Err("unclosed object".into()); }
                    let k = self.name()?;
                    self.expect(b':')?;
                    out.insert(k, self.value()?);
                }
                self.pos += 1;
                Ok(Value::Object(out))
            }
            Some(c) if c == b'-' || c.is_ascii_digit() => {
                let start = self.pos;
                self.pos += 1;
                while self.pos < self.src.len() && (self.src[self.pos].is_ascii_digit() || b".eE+-".contains(&self.src[self.pos])) {
                    self.pos += 1;
                }
                serde_json::from_slice(&self.src[start..self.pos]).map_err(|e| format!("bad number: {e}"))
            }
            Some(_) => match self.name()?.as_str() {
                "true" => Ok(Value::Bool(true)),
                "false" => Ok(Value::Bool(false)),
                "null" => Ok(Value::Null),
                other => Ok(Value::String(other.to_string())), // an enum value
            },
            None => Err("expected a value".into()),
        }
    }

    fn selection(&mut self) -> Result<Vec<Field>, String> {
        self.expect(b'{')?;
        let mut out = Vec::new();
        while self.peek() != Some(b'}') {
            if self.peek().is_none() { return Err("unclosed selection set".into()); }
            if self.src[self.pos..].starts_with(b"...") { return Err("fragments are not supported".into()); }
            let first = self.name()?;
            let (key, name) = if self.peek() == Some(b':') { self.pos += 1; (first, self.name()?) } else { (first.clone(), first) };
            if name.starts_with("__") { return Err(format!("introspection ({name}) is not supported; GET /graphql/schema has the SDL")); }
            let mut args = Map::new();
            if self.peek() == Some(b'(') {
                self.pos += 1;
                while self.peek() != Some(b')') {
                    if self.peek().is_none() { return Err("unclosed arguments".into()); }
                    let k = self.name()?;
                    self.expect(b':')?;
                    args.insert(k, self.value()?);
                }
                self.pos += 1;
            }
            if self.peek() == Some(b'@') { return Err("directives are not supported".into()); }
            let selection = if self.peek() == Some(b'{') { self.selection()? } else { Vec::new() };
            out.push(Field { key, name, args, selection });
        }
        self.pos += 1;
        Ok(out)
    }
}

/// Parse one operation; variables are substituted as they are read.
pub fn parse(query: &str, vars: &Map<String, Value>) -> Result<Operation, String> {
    let mut p = Parser { src: query.as_bytes(), pos: 0, vars };
    let mut kind = "query".to_string();
    if p.peek() != Some(b'{') {
        kind = p.name()?;
        if kind == "subscription" { return Err("subscriptions are not supported: use hub.subscribe over the WebSocket".into()); }
        if kind != "query" && kind != "mutation" { return Err(format!("unknown operation '{kind}'")); }
        if p.peek() != Some(b'{') && p.peek() != Some(b'(') { p.name()?; }
        if p.peek() == Some(b'(') {
            // Variable definitions: their values come from `variables`; the declarations are skipped.
            let mut depth = 0;
            while let Some(c) = p.src.get(p.pos).copied() {
                p.pos += 1;
                if c == b'(' { depth += 1; }
                if c == b')' { depth -= 1; if depth == 0 { break; } }
            }
        }
    }
    let selection = p.selection()?;
    if p.peek().is_some() { return Err("one operation per document".into()); }
    Ok(Operation { kind, selection })
}

/// Keep only the selected keys of `v` (of every object in a list), recursively.
pub fn project(v: &Value, selection: &[Field]) -> Value {
    if selection.is_empty() { return v.clone(); }
    match v {
        Value::Array(items) => Value::Array(items.iter().map(|i| project(i, selection)).collect()),
        Value::Object(m) => Value::Object(selection.iter()
            .map(|f| (f.key.clone(), m.get(&f.name).map(|x| project(x, &f.selection)).unwrap_or(Value::Null)))
            .collect()),
        other => other.clone(),
    }
}

/// The GraphQL name of a hub method: its last segment.
pub fn field_name(method: &str) -> String {
    method.rsplit('.').next().unwrap_or(method).to_string()
}

/// Whether a method acts on a game rather than reading: such a method is a Mutation field.
pub fn is_mutation(method: &str) -> bool {
    method == "hub.call"
}

/// The SDL of the schema, derived from the method table: one root field per method that can be
/// answered per request (subscriptions stream over the WebSocket), its params as JSON arguments.
pub fn sdl(methods: &Value) -> String {
    let (mut q, mut m) = (Vec::new(), Vec::new());
    for meth in methods.as_array().into_iter().flatten() {
        let Some(name) = meth.get("method").and_then(Value::as_str) else { continue };
        if name.ends_with("subscribe") { continue; }
        let args: Vec<String> = meth.get("params").and_then(Value::as_object).map(|p| p.keys().map(|k| format!("{k}: JSON")).collect()).unwrap_or_default();
        let line = format!("  \"\"\"{name}\"\"\"\n  {}{}: JSON", field_name(name), if args.is_empty() { String::new() } else { format!("({})", args.join(", ")) });
        if is_mutation(name) { m.push(line) } else { q.push(line) }
    }
    format!("scalar JSON\n\ntype Query {{\n{}\n}}\n\ntype Mutation {{\n{}\n}}\n", q.join("\n"), m.join("\n"))
}

/// Execute a GraphQL request through `rpc`, the hub's own JSON-RPC dispatch. Returns
/// `{data, errors?}` as GraphQL does: a failing field is null with an error naming its path.
pub fn execute(request: &Value, methods: &Value, rpc: &dyn Fn(&str, Value) -> Result<Value, String>) -> Value {
    let query = request.get("query").and_then(Value::as_str).unwrap_or("");
    let empty = Map::new();
    let vars = request.get("variables").and_then(Value::as_object).unwrap_or(&empty);
    let op = match parse(query, vars) {
        Ok(op) => op,
        Err(e) => return json!({"data": null, "errors": [{"message": e}]}),
    };
    let known: Vec<String> = methods.as_array().into_iter().flatten()
        .filter_map(|m| m.get("method").and_then(Value::as_str).map(str::to_string))
        .filter(|m| !m.ends_with("subscribe")).collect();
    let mut data = Map::new();
    let mut errors = Vec::new();
    for f in &op.selection {
        let Some(method) = known.iter().find(|m| field_name(m) == f.name) else {
            errors.push(json!({"message": format!("no field '{}' (GET /graphql/schema)", f.name), "path": [f.key]}));
            data.insert(f.key.clone(), Value::Null);
            continue;
        };
        if is_mutation(method) != (op.kind == "mutation") {
            errors.push(json!({"message": format!("'{}' is a {} field", f.name, if is_mutation(method) { "Mutation" } else { "Query" }), "path": [f.key]}));
            data.insert(f.key.clone(), Value::Null);
            continue;
        }
        match rpc(method, Value::Object(f.args.clone())) {
            Ok(v) => { data.insert(f.key.clone(), project(&v, &f.selection)); }
            Err(e) => {
                errors.push(json!({"message": e, "path": [f.key]}));
                data.insert(f.key.clone(), Value::Null);
            }
        }
    }
    if errors.is_empty() { json!({"data": data}) } else { json!({"data": data, "errors": errors}) }
}
