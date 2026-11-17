//! RESEARCH-ONLY control panel for OpenTTD's embedded MCP server (ADR 0008, issue I0035): the Rust
//! port of tools/research/mcp-panel/server.py, served by `mcp-panel` (src/bin/mcp-panel.rs).
//!
//! Starts a headless dedicated server from an MCP-enabled build, owns its console (stdin), starts the
//! MCP server and, optionally, a company AI, and serves a local control window at
//! http://127.0.0.1:<panel-port>/ that drives the game through MCP. The bearer token lives in this
//! process only: it is redacted from the console log the window shows and never written to disk.
//! Everything binds to loopback.
//!
//! One declaration, every surface: the router dispatches from `routes()`, `/openapi.json` documents
//! the same table, and the routes derived from the live MCP catalogue (`derive_routes`) are added at
//! request time from the same tools/list and resources/list. Every POST body is checked against its
//! schema (`body_errors`) before a handler runs. One event bus (`Bus`) feeds REST (/api/events), SSE
//! (/api/events/stream) and Rhai; Rhai scripts (/api/rhai, /api/rhai/stream) run in this process,
//! in the engine mcp-rhai runs them in (src/main.rs `host`).
//!
//! JSON answers are written as Python's json.dumps writes them (", " and ": ", non-ASCII escaped):
//! launchers match `"mcp_up": true` in /api/status text.

use serde_json::{json, Map, Value};
use std::collections::{BTreeMap, HashSet, VecDeque};
use std::io::{BufRead, BufReader, Read, Write};
use std::net::{TcpListener, TcpStream};
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{mpsc, Arc, Condvar, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use crate::hub;
use crate::panel_bot;
use crate::panel_decision;
use crate::panel_results;
use crate::rhai_host;

/// The panel's own directory (static pages, Rhai library, npm packages).
pub fn here() -> PathBuf {
    PathBuf::from(concat!(env!("CARGO_MANIFEST_DIR"), "/../mcp-panel"))
}

fn statics() -> PathBuf {
    here().join("static")
}

/// A sibling binary of this crate (mcp-plan, mcp-intent, mcp-graph): next to this executable first
/// (any CARGO_TARGET_DIR), else where server.py looked for it.
pub fn sibling(name: &str) -> PathBuf {
    if let Some(p) = std::env::current_exe().ok().and_then(|e| e.parent().map(|d| d.join(name))) {
        if p.is_file() {
            return p;
        }
    }
    here().join("../mcp-rhai/target/release").join(name)
}

const BUILD_IT: &str = "build it: cargo build --release in tools/research/mcp-rhai";

// ---------------------------------------------------------------------- arguments

#[derive(Clone, Debug, PartialEq)]
pub struct Args {
    pub bin: String,
    pub panel_port: u16,
    pub mcp_port: u16,
    pub game_port: u16,
    pub seed: i64,
    pub ai: String,
    pub auto_approve: bool,
    pub plan_bot: bool,
}

pub const USAGE: &str = "usage: mcp-panel --bin <openttd> [--panel-port 8800] [--mcp-port 8731] [--game-port 3979]\n\
                         \x20                [--seed 1] [--ai PrismaticAI] [--auto-approve] [--no-plan-bot]";

/// server.py's command line: `--name value` or `--name=value`.
pub fn parse_args(argv: &[String]) -> Result<Args, String> {
    let mut a = Args { bin: String::new(), panel_port: 8800, mcp_port: 8731, game_port: 3979, seed: 1,
                       ai: "PrismaticAI".into(), auto_approve: false, plan_bot: true };
    let mut bin = None;
    let mut i = 0;
    while i < argv.len() {
        let (name, inline) = match argv[i].split_once('=') {
            Some((n, v)) if n.starts_with("--") => (n.to_string(), Some(v.to_string())),
            _ => (argv[i].clone(), None),
        };
        let value = |i: &mut usize| -> Result<String, String> {
            if let Some(v) = inline.clone() {
                return Ok(v);
            }
            *i += 1;
            argv.get(*i).cloned().ok_or_else(|| format!("argument {name}: expected one argument"))
        };
        let port = |v: String, n: &str| v.parse::<u16>().map_err(|_| format!("argument {n}: invalid int value: '{v}'"));
        match name.as_str() {
            "--bin" => bin = Some(value(&mut i)?),
            "--panel-port" => a.panel_port = port(value(&mut i)?, "--panel-port")?,
            "--mcp-port" => a.mcp_port = port(value(&mut i)?, "--mcp-port")?,
            "--game-port" => a.game_port = port(value(&mut i)?, "--game-port")?,
            "--seed" => {
                let v = value(&mut i)?;
                a.seed = v.parse().map_err(|_| format!("argument --seed: invalid int value: '{v}'"))?;
            }
            "--ai" => a.ai = value(&mut i)?,
            "--auto-approve" => a.auto_approve = true,
            "--no-plan-bot" => a.plan_bot = false,
            other => return Err(format!("unrecognized arguments: {other}")),
        }
        i += 1;
    }
    a.bin = bin.ok_or("the following arguments are required: --bin")?;
    Ok(a)
}

// ---------------------------------------------------------------------- JSON as Python writes it

struct PyFormatter;

impl serde_json::ser::Formatter for PyFormatter {
    fn begin_array_value<W: ?Sized + Write>(&mut self, w: &mut W, first: bool) -> std::io::Result<()> {
        if first { Ok(()) } else { w.write_all(b", ") }
    }
    fn begin_object_key<W: ?Sized + Write>(&mut self, w: &mut W, first: bool) -> std::io::Result<()> {
        if first { Ok(()) } else { w.write_all(b", ") }
    }
    fn begin_object_value<W: ?Sized + Write>(&mut self, w: &mut W) -> std::io::Result<()> {
        w.write_all(b": ")
    }
    fn write_string_fragment<W: ?Sized + Write>(&mut self, w: &mut W, fragment: &str) -> std::io::Result<()> {
        for c in fragment.chars() {
            if c.is_ascii() {
                w.write_all(&[c as u8])?;
            } else {
                let mut units = [0u16; 2];
                for u in c.encode_utf16(&mut units) {
                    write!(w, "\\u{u:04x}")?;
                }
            }
        }
        Ok(())
    }
}

/// `json.dumps(v)`: ", " and ": " separators, non-ASCII as \uXXXX.
pub fn py_json(v: &Value) -> String {
    let mut out = Vec::new();
    let mut ser = serde_json::Serializer::with_formatter(&mut out, PyFormatter);
    serde::Serialize::serialize(v, &mut ser).expect("a Value always serialises");
    String::from_utf8(out).expect("the formatter writes ASCII")
}

/// A value as a Python f-string shows it: a string as it is, null as None, booleans as True/False.
pub fn py_str(v: Option<&Value>) -> String {
    match v {
        None | Some(Value::Null) => "None".into(),
        Some(Value::String(s)) => s.clone(),
        Some(Value::Bool(b)) => if *b { "True".into() } else { "False".into() },
        Some(Value::Number(n)) => n.to_string(),
        Some(other) => py_repr(other),
    }
}

/// `repr()` of a JSON value: strings quoted the way Python quotes them.
pub fn py_repr(v: &Value) -> String {
    match v {
        Value::String(s) => {
            let quote = if s.contains('\'') && !s.contains('"') { '"' } else { '\'' };
            let mut out = String::from(quote);
            for c in s.chars() {
                match c {
                    '\\' => out.push_str("\\\\"),
                    '\n' => out.push_str("\\n"),
                    '\t' => out.push_str("\\t"),
                    '\r' => out.push_str("\\r"),
                    c if c == quote => { out.push('\\'); out.push(c); }
                    c => out.push(c),
                }
            }
            out.push(quote);
            out
        }
        Value::Array(a) => format!("[{}]", a.iter().map(py_repr).collect::<Vec<_>>().join(", ")),
        Value::Object(m) => format!("{{{}}}", m.iter().map(|(k, v)| format!("{}: {}", py_repr(&json!(k)), py_repr(v)))
            .collect::<Vec<_>>().join(", ")),
        other => py_str(Some(other)),
    }
}

/// Python truthiness of a JSON value.
fn truthy(v: Option<&Value>) -> bool {
    match v {
        None | Some(Value::Null) => false,
        Some(Value::Bool(b)) => *b,
        Some(Value::Number(n)) => n.as_f64().map_or(true, |f| f != 0.0),
        Some(Value::String(s)) => !s.is_empty(),
        Some(Value::Array(a)) => !a.is_empty(),
        Some(Value::Object(m)) => !m.is_empty(),
    }
}

/// `body or {}`.
fn or_empty(v: &Value) -> Value {
    if truthy(Some(v)) { v.clone() } else { json!({}) }
}

/// `int(v)` for a JSON value: numbers truncated, booleans as 0/1, numeric strings parsed.
pub fn py_int(v: &Value) -> Result<i64, String> {
    match v {
        Value::Bool(b) => Ok(i64::from(*b)),
        Value::Number(n) => n.as_i64().or_else(|| n.as_f64().map(|f| f.trunc() as i64)).ok_or_else(|| format!("int() of {n}")),
        Value::String(s) => s.trim().parse::<i64>().map_err(|_| format!("invalid literal for int() with base 10: {}", py_repr(v))),
        other => Err(format!("int() argument must be a string or a number, not {}", py_repr(other))),
    }
}

/// Seconds as Python's `float()` reads a JSON number (default when absent).
fn py_float(v: Option<&Value>, default: f64) -> Result<f64, String> {
    match v {
        None => Ok(default),
        Some(Value::Number(n)) => Ok(n.as_f64().unwrap_or(default)),
        Some(Value::Bool(b)) => Ok(if *b { 1.0 } else { 0.0 }),
        Some(Value::String(s)) => s.trim().parse().map_err(|_| format!("could not convert string to float: {}", py_repr(&json!(s)))),
        Some(other) => Err(format!("float() argument must be a string or a real number, not {}", py_repr(other))),
    }
}

/// `text[:n]`, by characters.
fn head(text: &str, n: usize) -> String {
    text.chars().take(n).collect()
}

// ---------------------------------------------------------------------- time

fn strftime_local(fmt: &str) -> String {
    let c = std::ffi::CString::new(fmt).unwrap_or_default();
    let mut buf = [0u8; 64];
    // SAFETY: localtime_r and strftime write into the local, correctly sized buffers.
    let n = unsafe {
        let t = libc::time(std::ptr::null_mut());
        let mut tm: libc::tm = std::mem::zeroed();
        libc::localtime_r(&t, &mut tm);
        libc::strftime(buf.as_mut_ptr().cast(), buf.len(), c.as_ptr(), &tm)
    };
    String::from_utf8_lossy(&buf[..n]).into_owned()
}

fn unix_now() -> f64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs_f64()).unwrap_or(0.0)
}

// ---------------------------------------------------------------------- the event bus

/// The event bus every surface reads: REST (/api/events), SSE (/api/events/stream), Rhai. Each event
/// is `{seq, time, kind, data}`; the newest 5000 are kept.
pub struct Bus {
    inner: Mutex<(u64, VecDeque<Value>)>,
    cv: Condvar,
}

pub const BUS_KEEP: usize = 5000;

/// Does an event kind pass a filter? A filter entry matches its kind and every kind below it
/// (`game` takes `game.month`); no filter takes everything.
pub fn kind_matches(kind: &str, kinds: &[String]) -> bool {
    kinds.is_empty() || kinds.iter().any(|k| kind == k || (kind.starts_with(k.as_str()) && kind[k.len()..].starts_with('.')))
}

/// `?kinds=a,b` as a list, empty entries dropped.
pub fn kinds_of(text: &str) -> Vec<String> {
    text.split(',').filter(|k| !k.is_empty()).map(str::to_string).collect()
}

impl Default for Bus {
    fn default() -> Self {
        Self::new()
    }
}

impl Bus {
    pub fn new() -> Bus {
        Bus { inner: Mutex::new((0, VecDeque::new())), cv: Condvar::new() }
    }

    pub fn publish(&self, kind: &str, data: Value) -> u64 {
        let mut g = self.inner.lock().unwrap();
        g.0 += 1;
        let seq = g.0;
        g.1.push_back(json!({"seq": seq, "time": strftime_local("%Y-%m-%dT%H:%M:%S%z"), "kind": kind, "data": data}));
        while g.1.len() > BUS_KEEP {
            g.1.pop_front();
        }
        self.cv.notify_all();
        seq
    }

    pub fn seq(&self) -> u64 {
        self.inner.lock().unwrap().0
    }

    /// The first `limit` events after `since` that pass `kinds`.
    pub fn since(&self, since: u64, kinds: &[String], limit: usize) -> Vec<Value> {
        let g = self.inner.lock().unwrap();
        g.1.iter().filter(|e| e["seq"].as_u64().unwrap_or(0) > since && kind_matches(e["kind"].as_str().unwrap_or(""), kinds))
            .take(limit).cloned().collect()
    }

    /// Wait until an event after `since` exists or `timeout` passes; the newest sequence number.
    pub fn wait_past(&self, since: u64, timeout: Duration) -> u64 {
        let g = self.inner.lock().unwrap();
        let (g, _) = self.cv.wait_timeout_while(g, timeout, |g| g.0 <= since).unwrap();
        g.0
    }

    /// One step of a stream: the events after `since` that pass `kinds`, among the next 500 of all
    /// kinds, and the sequence number the stream continues from.
    pub fn stream_step(&self, since: u64, kinds: &[String]) -> (Vec<Value>, u64) {
        let all = self.since(since, &[], 500);
        let next = all.last().and_then(|e| e["seq"].as_u64()).unwrap_or(since).max(since);
        let batch = all.into_iter().filter(|e| kind_matches(e["kind"].as_str().unwrap_or(""), kinds)).collect();
        (batch, next)
    }
}

// ---------------------------------------------------------------------- console lines

/// The console's token line, whatever its wording: "Bearer token (<anything>): <32+ hex digits>".
/// Returns (start, end, token) of the matched text.
pub fn find_token(line: &str) -> Option<(usize, usize, String)> {
    let mut from = 0;
    while let Some(p) = line[from..].find("Bearer token (") {
        let start = from + p;
        let after = start + "Bearer token (".len();
        if let Some(close) = line[after..].find(')') {
            let rest = &line[after + close..];
            if let Some(hex) = rest.strip_prefix("): ") {
                let n = hex.chars().take_while(char::is_ascii_hexdigit).count();
                if n >= 32 {
                    let end = after + close + 3 + n;
                    return Some((start, end, hex[..n].to_string()));
                }
            }
        }
        from = after;
    }
    None
}

/// A line as the window shows it: the token line reworded, and the token itself nowhere.
pub fn redact(line: &str, token: Option<&str>) -> (String, Option<String>) {
    let mut out = line.to_string();
    let mut found = None;
    if let Some((s, e, t)) = find_token(line) {
        out = format!("{}Bearer token (held by the panel): <REDACTED>{}", &line[..s], &line[e..]);
        found = Some(t);
    }
    if let Some(t) = found.as_deref().or(token) {
        out = out.replace(t, "<REDACTED>");
    }
    (out, found)
}

/// "denied #<n>" at the start of a line: an approval withdrawn or refused in the game.
pub fn denied_id(line: &str) -> Option<i64> {
    let rest = line.strip_prefix("denied #")?;
    let n = rest.chars().take_while(char::is_ascii_digit).count();
    let next = rest[n..].chars().next();
    if n == 0 || next.map_or(false, |c| c.is_alphanumeric() || c == '_') {
        return None;
    }
    rest[..n].parse().ok()
}

/// The ~ console's plan verb: "[prismatic-plan] <n> <text>".
pub fn plan_request(line: &str) -> Option<(i64, String)> {
    let mut from = 0;
    while let Some(p) = line[from..].find("[prismatic-plan] ") {
        let at = from + p + "[prismatic-plan] ".len();
        let rest = &line[at..];
        let n = rest.chars().take_while(char::is_ascii_digit).count();
        if n > 0 {
            if let Some(text) = rest[n..].strip_prefix(' ') {
                if !text.is_empty() {
                    if let Ok(id) = rest[..n].parse() {
                        return Some((id, text.to_string()));
                    }
                }
            }
        }
        from = at;
    }
    None
}

/// An AI log line: "[script:<n>] [<company>] [<S|E|P|W|I>] <text>".
pub fn ai_log(line: &str) -> Option<(i64, String, String)> {
    let mut from = 0;
    while let Some(p) = line[from..].find("[script:") {
        let at = from + p + "[script:".len();
        from = at;
        let rest = &line[at..];
        let n = rest.chars().take_while(char::is_ascii_digit).count();
        let Some(rest) = rest[n..].strip_prefix("] [").filter(|_| n > 0) else { continue };
        let c = rest.chars().take_while(char::is_ascii_digit).count();
        let Some(after) = rest[c..].strip_prefix("] [").filter(|_| c > 0) else { continue };
        let mut chars = after.chars();
        let Some(level) = chars.next().filter(|l| "SEPWI".contains(*l)) else { continue };
        let Some(text) = chars.as_str().strip_prefix("] ") else { continue };
        let level = match level {
            'S' => "squirrel_error", 'E' => "error", 'P' => "squirrel_info", 'W' => "warning", _ => "info",
        };
        return Some((rest[..c].parse().ok()?, level.to_string(), text.to_string()));
    }
    None
}

/// The value in the console's answer to `setting difficulty.max_loan` ("... max_loan' is '300000'").
pub fn max_loan_of(line: &str) -> Option<i64> {
    let mut from = 0;
    while let Some(p) = line[from..].find("max_loan") {
        let mut rest = &line[from + p + "max_loan".len()..];
        from += p + 1;
        rest = rest.strip_prefix('\'').unwrap_or(rest);
        let Some(r) = rest.strip_prefix(" is") else { continue };
        let r = r.strip_prefix(':').unwrap_or(r);
        let Some(r) = r.strip_prefix(' ') else { continue };
        let r = r.strip_prefix('\'').unwrap_or(r);
        let n = r.chars().take_while(char::is_ascii_digit).count();
        if n > 0 {
            return r[..n].parse().ok();
        }
    }
    None
}

/// The console's `companies` line: "#:1(Blue) Company Name: '...'  Year Founded: 1950  Money: 5694
/// Loan: 100000 ..." as (company number, money, loan).
pub fn company_row(line: &str) -> Option<(i64, i64, i64)> {
    let rest = &line[line.find("#:")? + 2..];
    let n = rest.chars().take_while(char::is_ascii_digit).count();
    let id: i64 = rest[..n].parse().ok()?;
    let rest = rest[n..].strip_prefix('(')?;
    let rest = &rest[rest.find(')')? + 1..];
    let rest = rest.trim_start().strip_prefix("Company Name: '")?;
    let rest = &rest[rest.find('\'')? + 1..];
    let rest = rest.trim_start().strip_prefix("Year Founded: ")?;
    let y = rest.chars().take_while(char::is_ascii_digit).count();
    let rest = rest[y..].trim_start().strip_prefix("Money: ")?;
    let m = rest.chars().enumerate().take_while(|(i, c)| c.is_ascii_digit() || (*i == 0 && *c == '-')).count();
    let money: i64 = rest[..m].parse().ok()?;
    let rest = rest[m..].trim_start().strip_prefix("Loan: ")?;
    let l = rest.chars().take_while(char::is_ascii_digit).count();
    Some((id, money, rest[..l].parse().ok()?))
}

/// A console line that approves: approvals are the owner's act, through POST /api/approve.
pub fn is_approval_line(cmd: &str) -> bool {
    let low = cmd.trim_start().to_ascii_lowercase();
    let Some(rest) = low.strip_prefix("prismatic_mcp").or_else(|| low.strip_prefix("mcp")) else { return false };
    if !rest.starts_with(char::is_whitespace) {
        return false;
    }
    let rest = rest.trim_start();
    ["approve", "auto_approve", "auto-approve"].iter().any(|w| {
        rest.strip_prefix(w).map_or(false, |after| !after.starts_with(|c: char| c.is_alphanumeric() || c == '_'))
    })
}

// ---------------------------------------------------------------------- subprocesses

pub struct Run {
    pub code: i32,
    pub stdout: String,
    pub stderr: String,
}

/// Run a command to its end within `timeout`, its output captured (stdout to `stdout_file` when
/// given), `input` on its stdin.
pub fn run(cmd: &mut Command, input: Option<Vec<u8>>, timeout: Duration, stdout_file: Option<std::fs::File>) -> Result<Run, String> {
    let shown = format!("{cmd:?}");
    cmd.stdin(if input.is_some() { Stdio::piped() } else { Stdio::null() }).stderr(Stdio::piped());
    match stdout_file {
        Some(f) => cmd.stdout(f),
        None => cmd.stdout(Stdio::piped()),
    };
    let mut child = cmd.spawn().map_err(|e| format!("{shown}: {e}"))?;
    if let (Some(data), Some(mut stdin)) = (input, child.stdin.take()) {
        thread::spawn(move || { let _ = stdin.write_all(&data); });
    }
    let reader = |r: Option<Box<dyn Read + Send>>| thread::spawn(move || {
        let mut s = Vec::new();
        if let Some(mut r) = r { let _ = r.read_to_end(&mut s); }
        String::from_utf8_lossy(&s).into_owned()
    });
    let out = reader(child.stdout.take().map(|s| Box::new(s) as Box<dyn Read + Send>));
    let err = reader(child.stderr.take().map(|s| Box::new(s) as Box<dyn Read + Send>));
    let deadline = Instant::now() + timeout;
    let status = loop {
        if let Some(s) = child.try_wait().map_err(|e| e.to_string())? { break s; }
        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            return Err(format!("Command {shown} timed out after {} seconds", timeout.as_secs_f64()));
        }
        thread::sleep(Duration::from_millis(20));
    };
    Ok(Run { code: status.code().unwrap_or(-1), stdout: out.join().unwrap_or_default(), stderr: err.join().unwrap_or_default() })
}

// ---------------------------------------------------------------------- MCP over HTTP

/// One JSON-RPC call on an MCP endpoint with a bearer token; the whole JSON-RPC answer. An HTTP
/// error status is an error, as urllib raises it.
pub fn mcp_post(endpoint: &str, token: &str, method: &str, params: Value, timeout: Duration) -> Result<Value, String> {
    let body = json!({"jsonrpc": "2.0", "id": 1, "method": method, "params": params});
    let r = ureq::post(endpoint).timeout(timeout)
        .set("Content-Type", "application/json").set("Accept", "application/json, text/event-stream")
        .set("Authorization", &format!("Bearer {token}"))
        .send_string(&body.to_string());
    match r {
        Ok(resp) => resp.into_json::<Value>().map_err(|e| e.to_string()),
        Err(ureq::Error::Status(code, resp)) => Err(format!("HTTP Error {code}: {}", resp.status_text())),
        Err(e) => Err(e.to_string()),
    }
}

/// Whether a failed MCP call is tried once more. The game's MCP server answers one request at a time,
/// and the main page fires many reads at once: now and then one connection is refused or dropped and
/// the page saw a 502 (/api/catalogue, /api/bots/choices, 2026-11-17). A read is tried again after any
/// transport error; any other method only when the connection never opened, so a command the server
/// may have received is never sent twice.
pub fn rpc_retryable(method: &str, kind: ureq::ErrorKind, attempt: u32) -> bool {
    let read = matches!(method, "initialize" | "tools/list" | "resources/list" | "resources/read" | "prompts/list" | "ping");
    attempt == 0 && (read || kind == ureq::ErrorKind::ConnectionFailed)
}

/// A token file `{agent_token, endpoint}` as `prismatic_mcp start <port> <file>` writes it.
fn token_file(path: &str) -> Result<(String, String), String> {
    let cfg: Value = serde_json::from_str(&std::fs::read_to_string(path).map_err(|e| format!("{path}: {e}"))?)
        .map_err(|e| format!("{path}: {e}"))?;
    let endpoint = cfg["endpoint"].as_str().ok_or_else(|| format!("{path}: no endpoint"))?.to_string();
    let token = cfg["agent_token"].as_str().ok_or_else(|| format!("{path}: no agent_token"))?.to_string();
    Ok((endpoint, token))
}

/// The JSON a tool or resource answer carries in its first text content, else None.
fn content_json(r: &Value, pointer: &str) -> Option<Value> {
    serde_json::from_str(r.pointer(pointer)?.as_str()?).ok()
}

/// The HTTP status of an MCP answer the panel passes on: 200 for content, 404 when the server does
/// not know the resource or tool (JSON-RPC -32602, e.g. a resource this build does not have), 502 for
/// any other JSON-RPC error. A tool's own refusal inside its content stays 200 with its body.
pub fn rpc_status(out: &Value) -> u16 {
    let Some(err) = out.get("error").filter(|e| e.is_object()) else { return 200 };
    if err.get("jsonrpc").is_none() {
        return 200;
    }
    match err.get("error").filter(|e| e.is_object()) {
        Some(rpc) if rpc.get("code").and_then(Value::as_i64) == Some(-32602) => 404,
        Some(_) => 502,
        None => 200,
    }
}

// ---------------------------------------------------------------------- the game

struct Console {
    stdin: Option<ChildStdin>,
    last: Option<Instant>,
}

#[derive(Default)]
struct Log {
    seq: u64,
    lines: VecDeque<(u64, String)>,
}

/// One dedicated server, its console and its MCP endpoint.
pub struct Game {
    pub args: Args,
    tmp: PathBuf,
    child: Mutex<Option<Child>>,
    running: AtomicBool,
    console_w: Mutex<Console>,
    token: Mutex<Option<String>>,
    log: Mutex<Log>,
    events: Mutex<VecDeque<Value>>,
    pub bus: Bus,
    pending: Mutex<BTreeMap<i64, Value>>,
    pub auto_approve: AtomicBool,
    pub mcp_up: AtomicBool,
    pub ai_started: AtomicBool,
    rpc_id: AtomicU64,
    server_tools: Mutex<(Vec<Value>, Option<Instant>)>,
    client_tools: Mutex<(Vec<Value>, Option<Instant>)>,
    owner_q: Mutex<Option<mpsc::Sender<(String, String)>>>,
    plans_seen: Mutex<HashSet<i64>>,
    // Answered questions already published as ask.answered, by (id, question): the game window
    // numbers its questions from 1 again when the GUI client restarts, so an id alone matched a
    // different, earlier question and the new answer was never published.
    asks_reported: Mutex<HashSet<String>>,
    view_token: Mutex<Option<String>>,
    /// The overview's history: one sample per game day, newest last (see `sample_of`).
    history: Mutex<VecDeque<Value>>,
}

pub const NO_CLIENT: &str = "no GUI client attached (POST /api/view/attach {token_file})";

impl Game {
    pub fn new(args: Args) -> Arc<Game> {
        let tmp = std::env::temp_dir().join(format!("openttd-mcp-panel.{}.{}", std::process::id(), args.panel_port));
        Arc::new(Game {
            auto_approve: AtomicBool::new(args.auto_approve),
            args, tmp,
            child: Mutex::new(None), running: AtomicBool::new(false),
            console_w: Mutex::new(Console { stdin: None, last: None }),
            token: Mutex::new(None), log: Mutex::new(Log::default()), events: Mutex::new(VecDeque::new()),
            bus: Bus::new(), pending: Mutex::new(BTreeMap::new()),
            mcp_up: AtomicBool::new(false), ai_started: AtomicBool::new(false), rpc_id: AtomicU64::new(0),
            server_tools: Mutex::new((vec![], None)), client_tools: Mutex::new((vec![], None)),
            owner_q: Mutex::new(None), plans_seen: Mutex::new(HashSet::new()), history: Mutex::new(VecDeque::new()),
            asks_reported: Mutex::new(HashSet::new()), view_token: Mutex::new(None),
        })
    }

    // ------------------------------------------------------------------ process
    pub fn start(self: &Arc<Self>) -> Result<(), String> {
        std::fs::create_dir_all(&self.tmp).map_err(|e| format!("{}: {e}", self.tmp.display()))?;
        let cfg = self.tmp.join("openttd.cfg");
        std::fs::write(&cfg, format!(
            "[network]\nserver_name = OpenTTD MCP research server\nserver_game_type = local\n\
             server_port = {}\nmax_companies = 15\n[difficulty]\nmax_no_competitors = 1\n\
             [script]\nscript_max_opcode_till_suspend = {}\n", self.args.game_port, SCRIPT_OPCODES))
            .map_err(|e| format!("{}: {e}", cfg.display()))?;
        let argv: Vec<String> = vec![self.args.bin.clone(), "-D".into(), format!("127.0.0.1:{}", self.args.game_port), "-x".into(),
                                     "-g".into(), "-G".into(), self.args.seed.to_string(), "-c".into(), cfg.display().to_string(),
                                     "-d".into(), "net=1,script=4".into()];
        self.event("server", &format!("starting {}", argv.join(" ")));
        // stdout and stderr in one pipe, in the order the game writes them (exec keeps the pid).
        let mut child = Command::new("/bin/sh").arg("-c").arg("exec \"$0\" \"$@\" 2>&1").args(&argv)
            .stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::null())
            .spawn().map_err(|e| format!("{}: {e}", self.args.bin))?;
        let stdout = child.stdout.take().ok_or("no stdout")?;
        self.console_w.lock().unwrap().stdin = child.stdin.take();
        *self.child.lock().unwrap() = Some(child);
        self.running.store(true, Ordering::SeqCst);
        let (tx, rx) = mpsc::channel();
        *self.owner_q.lock().unwrap() = Some(tx);
        let g = self.clone();
        thread::spawn(move || g.read_log(stdout));
        let g = self.clone();
        thread::spawn(move || g.bring_up());
        let g = self.clone();
        thread::spawn(move || g.auto_loop());
        let g = self.clone();
        thread::spawn(move || g.watch());
        let g = self.clone();
        thread::spawn(move || g.watch_news());
        let g = self.clone();
        thread::spawn(move || g.owner_console_loop(rx));
        Ok(())
    }

    pub fn server_running(&self) -> bool {
        self.running.load(Ordering::SeqCst)
    }

    fn read_log(self: Arc<Self>, stdout: std::process::ChildStdout) {
        let mut r = BufReader::new(stdout);
        let mut raw = Vec::new();
        loop {
            raw.clear();
            match r.read_until(b'\n', &mut raw) {
                Ok(0) | Err(_) => break,
                Ok(_) => {}
            }
            let text = String::from_utf8_lossy(&raw);
            let held = self.token.lock().unwrap().clone();
            let (line, found) = redact(text.trim_end(), held.as_deref());
            if let Some(t) = found {
                *self.token.lock().unwrap() = Some(t);
            }
            {
                let mut log = self.log.lock().unwrap();
                log.seq += 1;
                let seq = log.seq;
                log.lines.push_back((seq, line.clone()));
                while log.lines.len() > 5000 { log.lines.pop_front(); }
            }
            if let Some(id) = denied_id(&line) {
                // Withdrawn or refused in the game: no longer pending here either.
                self.pending.lock().unwrap().remove(&id);
            }
            if let Some((n, text)) = plan_request(&line) {
                if self.plans_seen.lock().unwrap().insert(n) {
                    let g = self.clone();
                    thread::spawn(move || g.answer_plan(n, &text));
                }
            }
            match ai_log(&line) {
                Some((company, level, text)) => { self.bus.publish("ai.log", json!({"company": company, "level": level, "text": text})); }
                None => { self.bus.publish("console.line", json!({"line": line})); }
            }
        }
        let code = self.child.lock().unwrap().as_mut().and_then(|c| c.wait().ok()).and_then(|s| s.code());
        self.running.store(false, Ordering::SeqCst);
        self.event("server", &format!("server process exited with {}", code.map_or("None".into(), |c| c.to_string())));
    }

    /// One console line. One at a time, 0.25 s apart: the dedicated server tests stdin with select()
    /// and reads it with std::getline (src/video/dedicated_v.cpp), so lines that arrive together sit
    /// in the stream buffer, invisible to select(), until the next write (seen: a plan answer's lines
    /// appeared minutes late). A gap longer than one game-loop pass gives each line its own read.
    pub fn console(&self, command: &str) -> Result<(), String> {
        let mut c = self.console_w.lock().unwrap();
        if let Some(last) = c.last {
            let wait = Duration::from_millis(250).saturating_sub(last.elapsed());
            if !wait.is_zero() { thread::sleep(wait); }
        }
        let stdin = c.stdin.as_mut().ok_or("the server's console is closed")?;
        stdin.write_all(format!("{command}\n").as_bytes()).and_then(|_| stdin.flush()).map_err(|e| e.to_string())?;
        c.last = Some(Instant::now());
        Ok(())
    }

    pub fn log_seq(&self) -> u64 {
        self.log.lock().unwrap().seq
    }

    pub fn log_after(&self, mark: u64) -> Vec<String> {
        self.log.lock().unwrap().lines.iter().filter(|(n, _)| *n > mark).map(|(_, l)| l.clone()).collect()
    }

    fn log_has(&self, needle: &str) -> bool {
        self.log.lock().unwrap().lines.iter().any(|(_, l)| l.contains(needle))
    }

    /// Print a line in the owner's ~ console (the GUI client), coloured by level, through
    /// openttd.view.console.print; queued so lines keep their order and a missing client costs nothing.
    pub fn to_owner_console(&self, text: &str, level: &str) {
        if self.view_token.lock().unwrap().is_none() {
            return;
        }
        if let Some(tx) = self.owner_q.lock().unwrap().as_ref() {
            let _ = tx.send((text.to_string(), level.to_string()));
        }
    }

    fn owner_console_loop(self: Arc<Self>, rx: mpsc::Receiver<(String, String)>) {
        for (text, level) in rx {
            // No client, or an older one without the tool: nothing to do.
            let _ = self.view_call("openttd.view.console.print", json!({"text": text, "level": level}));
        }
    }

    /// Answer a sentence typed into the ~ console (`plan <text>`) through the intent resolver, into
    /// the same console with echo: the reading, the first candidates, and for a build its settings.
    fn answer_plan(&self, n: i64, text: &str) {
        let say = |msg: &str| {
            let line = format!("[prismatic-plan] {n} {}", head(&msg.replace('"', "'"), 240));
            let _ = self.console(&format!("echo \"{line}\""));
            self.to_owner_console(&line, "plan");
        };
        let exe = sibling("mcp-intent");
        if !exe.exists() {
            return say("the intent resolver is not built (cargo build --release in tools/research/mcp-rhai)");
        }
        let mut cmd = Command::new(&exe);
        cmd.arg("--panel").arg(self.panel_url());
        if let Some(ml) = self.max_loan() {
            cmd.arg("--params").arg(json!({"budget": ml}).to_string());
        }
        cmd.arg(text);
        let Ok(r) = run(&mut cmd, None, Duration::from_secs(180), None) else { return };
        if r.code != 0 {
            return say(&format!("not understood: {}", head(r.stderr.trim(), 200)));
        }
        let Ok(out) = serde_json::from_str::<Value>(&r.stdout) else { return };
        let (i, a) = (&out["intent"], &out["answer"]);
        let names: Vec<String> = i["subjects"].as_array().into_iter().flatten().map(|x| py_str(x.get("name"))).collect();
        let ordering = if truthy(i.get("ordering")) { py_str(i.get("ordering")) } else { String::new() };
        let subjects = if names.is_empty() { "anything".to_string() } else { names.join(", ") };
        say(&format!("{} {ordering} {subjects} ({}): {}", py_str(i.get("action")), py_str(i.get("language")), py_str(a.get("how"))));
        let f0 = |v: &Value| format!("{:.0}", v.as_f64().unwrap_or(0.0));
        for (k, c) in a["candidates"].as_array().into_iter().flatten().take(3).enumerate() {
            let k = k + 1;
            if c.get("from").is_some() {
                say(&format!("{k}. {} ({}/month) -> {}, {}, {} tiles", py_str(c["from"].get("name")), f0(&c["from"]["production"]),
                             py_str(c["to"].get("name")), py_str(c.get("cargo")), f0(&c["distance"])));
            } else if c.get("production").is_some() {
                say(&format!("{k}. {} at ({},{}), {}/month", py_str(c.get("name")), py_str(c.get("x")), py_str(c.get("y")), f0(&c["production"])));
            } else {
                let pick = |a: &str, b: &str| -> Value {
                    [a, b].iter().map(|k| c.get(*k).cloned().unwrap_or(Value::Null)).find(|v| truthy(Some(v))).unwrap_or(json!({}))
                };
                let (f, t) = (pick("a", "producer"), pick("b", "acceptor"));
                let cargo = if truthy(c.get("cargo")) { py_str(c.get("cargo")) } else { "passengers".into() };
                say(&format!("{k}. {cargo} by {}: {} -> {}, score {}", py_str(c.get("mode")), py_str(f.get("name")), py_str(t.get("name")),
                             py_str(c.get("score"))));
            }
        }
        if truthy(a.get("build")) {
            say(&format!("build: settings {}; {}", py_json(&a["build"]["settings"]), py_str(a["build"].get("execution"))));
        }
        let count = a["candidates"].as_array().map_or(0, Vec::len);
        self.event("intent", &format!("~ plan #{n} {}: {count} candidate(s)", py_repr(&json!(text))));
    }

    fn bring_up(self: Arc<Self>) {
        // The console reads stdin only once the game loop runs; repeat until it answers.
        for _ in 0..90 {
            if !self.server_running() {
                return;
            }
            let _ = self.console(&format!("prismatic_mcp start {}", self.args.mcp_port));
            thread::sleep(Duration::from_secs(2));
            if self.token.lock().unwrap().is_some() || self.log_has("already running") {
                break;
            }
        }
        if self.token.lock().unwrap().is_none() {
            self.event("mcp", "the MCP server did not start; see the console log");
            return;
        }
        self.mcp_up.store(true, Ordering::SeqCst);
        self.event("mcp", &format!("MCP server up at {} (token held by the panel)", self.endpoint()));
        // The script budget is set in openttd.cfg before the world is made (SCRIPT_OPCODES): the setting
        // is NewgameOnly, so the console's `setting` here was refused and every game so far ran at the
        // default 10000. Read it back so the log says what the bots really get.
        let mark = self.log_seq();
        if self.console("setting script.script_max_opcode_till_suspend").is_ok() {
            thread::sleep(Duration::from_millis(600));
            if let Some(line) = self.log_after(mark).into_iter().find(|l| l.contains("script_max_opcode_till_suspend")) {
                self.event("server", &format!("script budget: {}", line.trim()));
            }
        }
        if !self.args.ai.is_empty() {
            // The bot starts with what the planner's portfolio chose (freight rail, passenger rail, air),
            // so its routes come from the one profitability model; without a plan it plays as before.
            let settings = panel_bot::planned_air(&self);
            let flat = settings.iter().map(|(k, v)| format!("{k}={}", py_str(Some(v)))).collect::<Vec<_>>().join(",");
            let _ = self.console(format!("start_ai {} {flat}", self.args.ai).trim());
            self.ai_started.store(true, Ordering::SeqCst);
            self.event("ai", &format!("start_ai {} {flat} sent: the company plays on its own", self.args.ai).replace("  ", " "));
        }
    }

    /// The most a new company can borrow (difficulty.max_loan), read from the console; None if unknown.
    pub fn max_loan(&self) -> Option<i64> {
        let mark = self.log_seq();
        self.console("setting difficulty.max_loan").ok()?;
        thread::sleep(Duration::from_millis(600));
        self.log_after(mark).iter().find_map(|l| max_loan_of(l))
    }

    pub fn stop(&self) {
        if self.server_running() {
            let _ = self.console("quit");
            let deadline = Instant::now() + Duration::from_secs(5);
            loop {
                let done = self.child.lock().unwrap().as_mut().map_or(true, |c| matches!(c.try_wait(), Ok(Some(_))));
                if done || !self.server_running() { break; }
                if Instant::now() >= deadline {
                    if let Some(c) = self.child.lock().unwrap().as_mut() { let _ = c.kill(); }
                    break;
                }
                thread::sleep(Duration::from_millis(50));
            }
        }
        let _ = std::fs::remove_dir_all(&self.tmp);
    }

    // ------------------------------------------------------------------ MCP
    pub fn endpoint(&self) -> String {
        format!("http://127.0.0.1:{}/mcp", self.args.mcp_port)
    }

    pub fn panel_url(&self) -> String {
        format!("http://127.0.0.1:{}", self.args.panel_port)
    }

    /// Raw MCP JSON-RPC with the panel's token: the whole answer; an HTTP error status as
    /// `{http_error, body}`.
    pub fn rpc(&self, method: &str, params: Option<Value>) -> Result<Value, String> {
        let token = self.token.lock().unwrap().clone().ok_or("MCP server not up yet")?;
        let id = self.rpc_id.fetch_add(1, Ordering::SeqCst) + 1;
        let mut body = json!({"jsonrpc": "2.0", "id": id, "method": method});
        if let Some(p) = params {
            body["params"] = p;
        }
        let mut attempt = 0;
        loop {
            let r = ureq::post(&self.endpoint()).timeout(Duration::from_secs(5))
                .set("Content-Type", "application/json").set("Accept", "application/json, text/event-stream")
                .set("Authorization", &format!("Bearer {token}"))
                .send_string(&body.to_string());
            match r {
                Ok(resp) => {
                    let text = resp.into_string().map_err(|e| e.to_string())?;
                    return serde_json::from_str(if text.is_empty() { "{}" } else { &text }).map_err(|e| e.to_string());
                }
                Err(ureq::Error::Status(code, resp)) => return Ok(json!({"http_error": code, "body": resp.into_string().unwrap_or_default()})),
                Err(e) if rpc_retryable(method, e.kind(), attempt) => {
                    attempt += 1;
                    thread::sleep(Duration::from_millis(150));
                }
                Err(e) => return Err(e.to_string()),
            }
        }
    }

    pub fn resource(&self, uri: &str) -> Result<Value, String> {
        let r = self.rpc("resources/read", Some(json!({"uri": uri})))?;
        Ok(content_json(&r, "/result/contents/0/text").unwrap_or_else(|| json!({"error": r})))
    }

    pub fn tool(&self, name: &str, arguments: Value) -> Result<Value, String> {
        let r = self.rpc("tools/call", Some(json!({"name": name, "arguments": arguments})))?;
        let Some(out) = content_json(&r, "/result/content/0/text") else { return Ok(json!({"error": r})) };
        // Any tool that answers pending_approval is tracked, so the operator sees it with an Approve
        // button and the panel can resubmit the same call once it is approved.
        if out.get("status").and_then(Value::as_str) == Some("pending_approval") {
            if let Some(aid) = out.get("approval_id").filter(|v| !v.is_null()).and_then(|v| py_int(v).ok()) {
                let fresh = {
                    let mut p = self.pending.lock().unwrap();
                    if p.contains_key(&aid) { false } else {
                        p.insert(aid, json!({"tool": name, "args": arguments, "created": unix_now()}));
                        true
                    }
                };
                if fresh {
                    self.event("mcp", &format!("{name}({}) -> pending approval #{aid}", py_json(&arguments)));
                }
            }
        }
        Ok(out)
    }

    pub fn tool_names(&self) -> HashSet<String> {
        self.rpc("tools/list", None).ok().and_then(|r| r.pointer("/result/tools").and_then(Value::as_array).cloned())
            .unwrap_or_default().iter().filter_map(|t| t["name"].as_str().map(str::to_string)).collect()
    }

    // ------------------------------------------------------------------ the owner's view (GUI client)
    pub fn view_token(&self) -> Option<String> {
        self.view_token.lock().unwrap().clone()
    }

    /// A tool call on the GUI client's own MCP server, from its token file (reread each call, so a
    /// restarted client is picked up).
    pub fn view_call(&self, tool: &str, arguments: Value) -> Result<Value, String> {
        let file = self.view_token().ok_or(NO_CLIENT)?;
        let (endpoint, token) = token_file(&file)?;
        let out = mcp_post(&endpoint, &token, "tools/call", json!({"name": tool, "arguments": or_empty(&arguments)}),
                           Duration::from_secs(10))?;
        Ok(content_json(&out, "/result/content/0/text").unwrap_or(out))
    }

    /// The game server's tools/list, cached for a few seconds.
    pub fn server_tools(&self) -> Vec<Value> {
        let mut c = self.server_tools.lock().unwrap();
        if c.1.map_or(true, |t| t.elapsed() > Duration::from_secs(5)) && self.mcp_up.load(Ordering::SeqCst) {
            if let Ok(r) = self.rpc("tools/list", None) {
                c.0 = r.pointer("/result/tools").and_then(Value::as_array).cloned().unwrap_or_default();
                c.1 = Some(Instant::now());
            }
        }
        c.0.clone()
    }

    /// The attached GUI client's tools/list, cached for a few seconds; [] without a client.
    pub fn client_tools(&self) -> Vec<Value> {
        let Some(file) = self.view_token() else { return vec![] };
        let mut c = self.client_tools.lock().unwrap();
        if c.1.map_or(true, |t| t.elapsed() > Duration::from_secs(5)) {
            // The client restarts: keep the last list.
            if let Ok(r) = token_file(&file).and_then(|(e, t)| mcp_post(&e, &t, "tools/list", json!({}), Duration::from_secs(3))) {
                c.0 = r.pointer("/result/tools").and_then(Value::as_array).cloned().unwrap_or_default();
                c.1 = Some(Instant::now());
            }
        }
        c.0.clone()
    }

    // ------------------------------------------------------------------ approvals
    pub fn set_pause(&self, pause: bool) -> Result<Value, String> {
        let out = self.tool("openttd.game.set_pause", json!({"pause": pause}))?;
        if out.get("status").and_then(Value::as_str) != Some("pending_approval") {
            self.event("mcp", &format!("set_pause({}) -> {} {}", if pause { "True" } else { "False" }, py_str(out.get("status")),
                                       out.get("command_result").map_or(String::new(), |v| py_str(Some(v)))));
        }
        Ok(out)
    }

    pub fn approve(&self, id: i64) -> Result<Value, String> {
        let Some(info) = self.pending.lock().unwrap().remove(&id) else {
            return Ok(json!({"error": format!("no pending approval #{id} known to the panel")}));
        };
        self.console(&format!("prismatic_mcp approve {id}"))?;
        self.event("operator", &format!("approved #{id} in the OpenTTD console"));
        thread::sleep(Duration::from_millis(500));
        let tool = info["tool"].as_str().unwrap_or("");
        let out = self.tool(tool, info["args"].clone())?;
        let opt = |k: &str| out.get(k).map_or(String::new(), |v| py_str(Some(v)));
        self.event("mcp", &format!("#{id} {tool} resubmitted -> {} {} verdict={}", py_str(out.get("status")), opt("command_result"), opt("verdict")));
        Ok(out)
    }

    pub fn pending_json(&self) -> Vec<Value> {
        self.pending.lock().unwrap().iter().map(|(k, v)| {
            let mut m = Map::new();
            m.insert("id".into(), json!(k));
            if let Some(o) = v.as_object() { m.extend(o.clone()); }
            Value::Object(m)
        }).collect()
    }

    fn auto_loop(self: Arc<Self>) {
        loop {
            thread::sleep(Duration::from_secs(1));
            if self.auto_approve.load(Ordering::SeqCst) {
                let ids: Vec<i64> = self.pending.lock().unwrap().keys().copied().collect();
                for id in ids {
                    let _ = self.approve(id);
                }
            }
        }
    }

    pub fn event(&self, kind: &str, text: &str) {
        {
            let mut ev = self.events.lock().unwrap();
            ev.push_front(json!({"t": strftime_local("%H:%M:%S"), "kind": kind, "text": text}));
            ev.truncate(200);
        }
        self.bus.publish(&format!("panel.{kind}"), json!({"text": text}));
    }

    pub fn events_json(&self) -> Vec<Value> {
        self.events.lock().unwrap().iter().cloned().collect()
    }

    /// The game window's questions and answers. Every answer not yet reported is published as
    /// ask.answered here, so a reader of GET /api/ask and the watcher see the same events.
    pub fn ask_answers(&self) -> Result<Value, String> {
        let out = self.view_call("openttd.view.ask.answers", json!({}))?;
        let fresh: Vec<Value> = {
            let mut seen = self.asks_reported.lock().unwrap();
            out["asks"].as_array().into_iter().flatten()
                .filter(|a| a.get("answer").map_or(false, |v| !v.is_null()))
                .filter(|a| seen.insert(format!("{}\u{1f}{}", a["id"], a.get("question").unwrap_or(&Value::Null))))
                .cloned().collect()
        };
        for a in fresh {
            self.bus.publish("ask.answered", a);
        }
        Ok(out)
    }

    /// The news the owner's window shows, as events news.shown: what the game reports to the player
    /// (new companies, first arrivals, subsidies, ...) and what any MCP caller posted to them.
    fn watch_news(self: Arc<Self>) {
        let mut seen: Option<HashSet<String>> = None;
        loop {
            thread::sleep(Duration::from_secs(2));
            if self.view_token().is_none() {
                continue;
            }
            // The client restarts: try again next round.
            let Ok(out) = self.view_call("openttd.view.news.list", json!({"limit": 30})) else { continue };
            let news = out["news"].as_array().cloned().unwrap_or_default();
            let _ = self.ask_answers();
            let key = |n: &Value| py_json(&json!([n["date"], n["type"], n["text"]]));
            match seen.as_mut() {
                // What was there before the panel attached is history, not news.
                None => seen = Some(news.iter().map(key).collect()),
                Some(s) => {
                    for n in news.iter().rev() {
                        if s.insert(key(n)) {
                            self.bus.publish("news.shown", n.clone());
                        }
                    }
                }
            }
        }
    }

    /// Derive game events from what MCP reports, by difference between two reads.
    fn watch(self: Arc<Self>) {
        let mut prev: Option<Snapshot> = None;
        loop {
            thread::sleep(Duration::from_secs(1));
            if !self.mcp_up.load(Ordering::SeqCst) {
                continue;
            }
            let (Ok(st), Ok(cs), Ok(ds)) = (self.resource("openttd://game/state"), self.resource("openttd://companies"),
                                            self.resource("openttd://decisions")) else { continue };
            let Some(cur) = Snapshot::of(&st, &cs, &ds) else { continue };
            for (kind, data) in derive_game_events(prev.as_ref(), &cur) {
                self.bus.publish(kind, data);
            }
            if let Some(sample) = sample_of(&st, &cs) { self.record_sample(sample); }
            prev = Some(cur);
        }
    }
}

/// Opcodes a script may run per tick before it is suspended: the game's maximum for
/// script.script_max_opcode_till_suspend (default 10000). The bot plans rail paths in Squirrel; at the
/// default a 240-tile search ate months of game time, and with eight lines handed over the bot built
/// one line every two to three months while its money waited (research/results duel-20261117T100202).
/// Scripts run on the server only, so this changes nothing clients compute.
pub const SCRIPT_OPCODES: u32 = 250000;

impl Game {
    /// Keeps one sample per game day (a later read of the same day replaces it), at most
    /// `HISTORY_MAX`; a tick lower than the last one is a new world, and the history starts again.
    fn record_sample(&self, sample: Value) {
        let mut h = self.history.lock().unwrap();
        push_sample(&mut h, sample);
    }

    /// The samples with a tick above `since`.
    pub fn history_since(&self, since: f64) -> Vec<Value> {
        self.history.lock().unwrap().iter().filter(|s| s["t"].as_f64().unwrap_or(0.0) > since).cloned().collect()
    }
}

pub const HISTORY_MAX: usize = 8000;

pub fn push_sample(h: &mut VecDeque<Value>, sample: Value) {
    let tick = sample["t"].as_f64().unwrap_or(0.0);
    if let Some(last) = h.back() {
        if last["t"].as_f64().unwrap_or(0.0) > tick { h.clear(); }
        else if last["date"] == sample["date"] { h.pop_back(); }
    }
    h.push_back(sample);
    while h.len() > HISTORY_MAX { h.pop_front(); }
}

/// The fleet as a player counts it: openttd://game/state's `fleet.total` (primary vehicles), and
/// only from a server older than that field the pool count `num_vehicles`, which also counts every
/// wagon, articulated part and effect (smoke, sparks) and so swings by dozens a day.
pub fn fleet_total(data: &Value) -> Value {
    data.pointer("/fleet/total").cloned().unwrap_or_else(|| data.get("num_vehicles").cloned().unwrap_or(Value::Null))
}

/// One overview sample from openttd://game/state and openttd://companies: tick, date, counts, the
/// fleet by type, and per company money, value, loan, net (money − loan) and fleet; None without
/// state data.
pub fn sample_of(st: &Value, cs: &Value) -> Option<Value> {
    let d = st.get("data")?;
    let companies: Vec<Value> = cs.pointer("/data/companies").and_then(Value::as_array).cloned().unwrap_or_default()
        .into_iter().map(|c| {
            let n = |k: &str| c.get(k).and_then(Value::as_f64).unwrap_or(0.0);
            json!({"id": c.get("id"), "name": c.get("name"), "is_ai": c.get("is_ai"), "money": n("money"),
                   "value": n("company_value"), "loan": n("loan"), "net": n("money") - n("loan"),
                   "fleet": c.pointer("/fleet/total").cloned().unwrap_or(Value::Null)})
        }).collect();
    Some(json!({"t": st.get("game_tick"), "date": st.get("game_date"), "vehicles": fleet_total(d),
                "fleet": d.get("fleet").cloned().unwrap_or(Value::Null),
                "towns": d.get("num_towns"), "paused": d.get("paused"), "companies": companies}))
}

fn h_history(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let since = i.query.get("since").and_then(|v| v.parse::<f64>().ok()).unwrap_or(-1.0);
    let samples = g.history_since(since);
    // The newest tick kept: a client whose cursor is past it saw an older world, and starts again.
    let last = g.history.lock().unwrap().back().map(|s| s["t"].clone()).unwrap_or(Value::Null);
    Ok((200, json!({"since": since, "last": last, "count": samples.len(), "samples": samples})))
}

/// What the watcher compares between two reads.
#[derive(Clone, Debug, PartialEq)]
pub struct Snapshot {
    date: Value,
    paused: Value,
    vehicles: Value,
    towns: Value,
    tick: Value,
    companies: BTreeMap<String, (Value, Value)>,
    decisions: BTreeMap<String, Value>,
}

/// A JSON object key as json.dumps writes a dict key: a string as it is, anything else as JSON.
fn id_key(v: &Value) -> String {
    v.as_str().map(str::to_string).unwrap_or_else(|| v.to_string())
}

impl Snapshot {
    /// From openttd://game/state, openttd://companies and openttd://decisions; None without state data.
    pub fn of(st: &Value, cs: &Value, ds: &Value) -> Option<Snapshot> {
        let data = st.get("data")?;
        let list = |v: &Value, k: &str| v.get("data").and_then(|d| d.get(k)).and_then(Value::as_array).cloned().unwrap_or_default();
        let mut companies = BTreeMap::new();
        for c in list(cs, "companies") {
            if let Some(id) = c.get("id") { companies.insert(id_key(id), (id.clone(), c.clone())); }
        }
        let mut decisions = BTreeMap::new();
        for d in list(ds, "decisions") {
            if let Some(id) = d.get("decision_id") { decisions.insert(id_key(id), d.clone()); }
        }
        let g = |v: &Value, k: &str| v.get(k).cloned().unwrap_or(Value::Null);
        Some(Snapshot { date: g(st, "game_date"), paused: g(data, "paused"), vehicles: fleet_total(data),
                        towns: g(data, "num_towns"), tick: g(st, "game_tick"), companies, decisions })
    }
}

/// The events between two reads of the game (the first read announces what it observes).
pub fn derive_game_events(prev: Option<&Snapshot>, cur: &Snapshot) -> Vec<(&'static str, Value)> {
    let mut out = Vec::new();
    let Some(prev) = prev else {
        out.push(("game.observed", json!({"date": cur.date, "tick": cur.tick, "vehicles": cur.vehicles,
                                          "companies": cur.companies.len(), "towns": cur.towns})));
        for (_, c) in cur.companies.values() {
            out.push(("company.observed", c.clone()));
        }
        return out;
    };
    if let (Some(a), Some(b)) = (cur.tick.as_f64(), prev.tick.as_f64()) {
        if a < b {
            out.push(("game.new_world", json!({"date": cur.date, "towns": cur.towns})));
        }
    }
    let month = |d: &Value| head(d.as_str().unwrap_or(""), 7);
    if month(&cur.date) != month(&prev.date) {
        let companies: Map<String, Value> = cur.companies.iter()
            .map(|(k, (_, c))| (k.clone(), json!({"money": c.get("money"), "value": c.get("company_value")}))).collect();
        out.push(("game.month", json!({"date": cur.date, "vehicles": cur.vehicles, "companies": companies})));
    }
    if cur.paused != prev.paused {
        out.push((if truthy(Some(&cur.paused)) { "game.paused" } else { "game.resumed" }, json!({"date": cur.date, "tick": cur.tick})));
    }
    if cur.vehicles != prev.vehicles {
        out.push(("vehicles.count", json!({"from": prev.vehicles, "to": cur.vehicles, "date": cur.date})));
    }
    for (k, (_, c)) in &cur.companies {
        if !prev.companies.contains_key(k) { out.push(("company.founded", c.clone())); }
    }
    for (k, (_, c)) in &prev.companies {
        if !cur.companies.contains_key(k) { out.push(("company.removed", c.clone())); }
    }
    for (k, (id, b)) in &cur.companies {
        if let Some((_, a)) = prev.companies.get(k) {
            if a.get("name") != b.get("name") && truthy(b.get("name")) {
                out.push(("company.renamed", json!({"id": id, "from": a.get("name"), "to": b.get("name")})));
            }
        }
    }
    for (k, d) in &cur.decisions {
        match prev.decisions.get(k) {
            None => out.push(("decision.recorded", d.clone())),
            Some(p) if p != d => out.push(("decision.updated", d.clone())),
            _ => {}
        }
    }
    out
}

// ---------------------------------------------------------------------- discovery (cluster)
// Every panel and every GUI client it attaches registers here; any panel lists the whole cluster and
// checks each entry with a live probe. One JSON file per instance (mcp-instance/v1), written
// atomically, 0600, removed when its panel stops; an entry whose owning process is gone is dropped.

pub fn registry() -> PathBuf {
    match std::env::var_os("PRISMATIC_MCP_REGISTRY").filter(|v| !v.is_empty()) {
        Some(d) => PathBuf::from(d),
        None => PathBuf::from(std::env::var("HOME").unwrap_or_default()).join(".local/state/prismatic-mcp/instances"),
    }
}

/// The record an instance writes for itself, schema mcp-instance/v1.
pub fn registry_record(name: &str, entry: &Value) -> Value {
    let mut m = Map::new();
    m.insert("schema".into(), json!("mcp-instance/v1"));
    m.insert("name".into(), json!(name));
    m.insert("owner_pid".into(), json!(std::process::id()));
    m.insert("registered".into(), json!(unix_now()));
    if let Some(sup) = supervisor() { m.insert("supervisor".into(), sup); }
    if let Some(e) = entry.as_object() { m.extend(e.clone()); }
    Value::Object(m)
}

/// Who supervises this process: `PRISMATIC_MCP_SUPERVISOR`, a JSON object (mcp-e2e sets it to the
/// Majordomus task it was started under), or None when it is unset or not an object.
pub fn supervisor() -> Option<Value> {
    std::env::var("PRISMATIC_MCP_SUPERVISOR").ok()
        .and_then(|s| serde_json::from_str::<Value>(&s).ok()).filter(Value::is_object)
}

pub fn registry_write(name: &str, entry: &Value) {
    use std::os::unix::fs::PermissionsExt;
    let dir = registry();
    let _ = std::fs::create_dir_all(&dir);
    let tmp = dir.join(format!(".{name}.tmp"));
    if std::fs::write(&tmp, py_json(&registry_record(name, entry))).is_ok() {
        let _ = std::fs::set_permissions(&tmp, std::fs::Permissions::from_mode(0o600));
        let _ = std::fs::rename(&tmp, dir.join(format!("{name}.json")));
    }
}

pub fn registry_remove(name: &str) {
    let _ = std::fs::remove_file(registry().join(format!("{name}.json")));
}

fn pid_alive(pid: i64) -> bool {
    // SAFETY: kill with signal 0 only checks that the process exists.
    pid > 0 && pid <= i64::from(i32::MAX) && unsafe { libc::kill(pid as libc::pid_t, 0) } == 0
}

/// The panel that holds a server's token: a server a panel started announces no token file (the
/// panel keeps the token in memory), and the panel's record names that server's endpoint as `mcp`.
pub fn owning_panel(records: &[Value], server: &Value) -> Option<String> {
    let endpoint = server.get("endpoint")?.as_str()?;
    records.iter()
        .find(|r| r.get("kind").and_then(Value::as_str) == Some("panel") && r.get("mcp").and_then(Value::as_str) == Some(endpoint))
        .and_then(|r| r.get("url")?.as_str().map(|u| u.trim_end_matches('/').to_string()))
}

/// Every instance in the registry, each checked with a live probe; records of dead owners removed.
/// A server without a token file is reached through the panel that owns it (`via`).
pub fn discover() -> Vec<Value> {
    let mut files: Vec<PathBuf> = std::fs::read_dir(registry()).map(|d| {
        d.filter_map(|e| e.ok().map(|e| e.path())).filter(|p| p.extension().and_then(|x| x.to_str()) == Some("json")).collect()
    }).unwrap_or_default();
    files.sort();
    let mut live = Vec::new();
    for f in files {
        let Ok(e) = std::fs::read_to_string(&f).map_err(|_| ()).and_then(|t| serde_json::from_str::<Value>(&t).map_err(|_| ())) else { continue };
        if !e.is_object() { continue; }
        if !e.get("owner_pid").and_then(|p| py_int(p).ok()).map_or(false, pid_alive) {
            let _ = std::fs::remove_file(&f); // its panel is gone
            continue;
        }
        live.push(e);
    }
    let all = live.clone();
    let mut out = Vec::new();
    for mut e in live {
        e["alive"] = json!(false);
        let via = if e.get("token_file").is_none() && e.get("kind").and_then(Value::as_str) != Some("panel") {
            owning_panel(&all, &e)
        } else { None };
        let probe = || -> Result<Value, String> {
            if e.get("kind").and_then(Value::as_str) == Some("panel") {
                let url = e["url"].as_str().ok_or("no url in the record")?;
                let st: Value = ureq::get(&format!("{url}/api/status")).timeout(Duration::from_secs(2)).call()
                    .map_err(|e| e.to_string())?.into_json().map_err(|e| e.to_string())?;
                Ok(json!({"alive": true, "mcp_up": st.get("mcp_up"), "ai": st.get("ai"), "game_port": st.get("game_port")}))
            } else if let Some(url) = &via {
                let st: Value = ureq::get(&format!("{url}/api/status")).timeout(Duration::from_secs(2)).call()
                    .map_err(|e| e.to_string())?.into_json().map_err(|e| e.to_string())?;
                Ok(json!({"alive": st.get("mcp_up") == Some(&json!(true)), "via": url}))
            } else {
                let file = e["token_file"].as_str().ok_or("no token_file in the record")?;
                let (endpoint, token) = token_file(file)?;
                let alive = mcp_post(&endpoint, &token, "ping", json!({}), Duration::from_secs(2))?.get("result").is_some();
                Ok(json!({"endpoint": endpoint, "alive": alive}))
            }
        };
        match probe() {
            Ok(Value::Object(m)) => { for (k, v) in m { e[k.as_str()] = v; } }
            Ok(_) => {}
            Err(err) => e["error"] = json!(err), // a probe failure is the answer
        }
        out.push(e);
    }
    out
}

/// One MCP JSON-RPC call on a named instance: a panel through its /api/rpc, a server a panel owns
/// through that panel, a client (or a server with its own token file) directly.
pub fn instance_call(name: &str, method: &str, params: &Value) -> Result<(u16, Value), String> {
    let Some(e) = discover().into_iter().find(|x| x["name"] == json!(name)) else {
        return Ok((404, json!({"error": format!("no instance {name}")})));
    };
    let params = or_empty(params);
    let panel = match e.get("kind").and_then(Value::as_str) {
        Some("panel") => Some(e["url"].as_str().ok_or("no url in the record")?.to_string()),
        _ => e.get("via").and_then(Value::as_str).map(str::to_string),
    };
    if let Some(url) = panel {
        let v = ureq::post(&format!("{url}/api/rpc")).timeout(Duration::from_secs(10))
            .set("Content-Type", "application/json")
            .send_string(&json!({"method": method, "params": params}).to_string())
            .map_err(|e| e.to_string())?.into_json().map_err(|e| e.to_string())?;
        return Ok((200, v));
    }
    let (endpoint, token) = token_file(e["token_file"].as_str().ok_or("no token_file in the record")?)?;
    Ok((200, mcp_post(&endpoint, &token, method, params, Duration::from_secs(10))?))
}

/// WindowClass numbers to names, derived from src/window_type.h (aliases kept as extra names), so the
/// windows openttd.ui.windows reports by number get the names the source gives them.
pub fn window_classes(header: &str) -> Result<Value, String> {
    let body = header.split_once("enum class WindowClass").ok_or("no enum class WindowClass")?.1;
    let body = body.split_once("\n};").map_or(body, |(b, _)| b);
    let mut text = String::new();
    let mut rest = body;
    // Comments out: /* ... */ first, then // to the end of the line.
    while let Some(p) = rest.find("/*") {
        text.push_str(&rest[..p]);
        rest = rest[p + 2..].split_once("*/").map_or("", |(_, r)| r);
    }
    text.push_str(rest);
    let text: String = text.lines().map(|l| l.split_once("//").map_or(l, |(a, _)| a)).collect::<Vec<_>>().join("\n");
    let ident_end = |s: &str| -> Option<String> {
        // The last identifier in s that starts with a capital letter, if it ends s.
        let t = s.trim_end();
        let start = t.rfind(|c: char| !(c.is_ascii_alphanumeric() || c == '_')).map_or(0, |p| p + 1);
        let id = &t[start..];
        (id.starts_with(|c: char| c.is_ascii_uppercase()) && id.chars().all(|c| c.is_ascii_alphanumeric())).then(|| id.to_string())
    };
    let mut names: BTreeMap<String, i64> = BTreeMap::new();
    let mut by_value: BTreeMap<i64, Vec<String>> = BTreeMap::new();
    let mut value = -1i64;
    let chunks: Vec<&str> = text.split(',').collect();
    for chunk in &chunks[..chunks.len().saturating_sub(1)] {
        let (lhs, expr) = match chunk.split_once('=') { Some((l, r)) => (l, Some(r.trim())), None => (*chunk, None) };
        let Some(name) = ident_end(lhs) else { continue };
        let v = match expr {
            Some(e) if e.starts_with("WindowClass::") => *names.get(&e["WindowClass::".len()..]).ok_or_else(|| format!("unknown {e}"))?,
            Some(e) => match e.strip_prefix("0x").or_else(|| e.strip_prefix("0X")) {
                Some(h) => i64::from_str_radix(h, 16).map_err(|_| format!("bad value {e}"))?,
                None => e.parse().map_err(|_| format!("bad value {e}"))?,
            },
            None => value + 1,
        };
        value = v;
        names.insert(name.clone(), v);
        by_value.entry(v).or_default().push(name);
    }
    let mut out = Map::new();
    for (v, ns) in by_value {
        let name = if ns.len() > 1 { ns.last() } else { ns.first() }.cloned().unwrap_or_default();
        out.insert(v.to_string(), json!({"name": name, "aliases": ns}));
    }
    Ok(Value::Object(out))
}

// ---------------------------------------------------------------------- the declared routes

/// What a handler gets: the query (GET) and the body (POST, `{}` when empty).
pub struct Input {
    pub query: Query,
    pub body: Value,
}

pub type Handler = fn(&Arc<Game>, &Input) -> Result<(u16, Value), String>;

pub struct Route {
    pub method: &'static str,
    pub path: &'static str,
    pub op_id: &'static str,
    pub summary: &'static str,
    pub needs_mcp: bool,
    pub schema: Option<Value>,
    pub handler: Handler,
}

fn obj() -> Value {
    json!({"type": "object"})
}

fn s(props: Value, required: &[&str]) -> Option<Value> {
    Some(json!({"type": "object", "properties": props, "required": required}))
}

fn ok(v: Value) -> Result<(u16, Value), String> {
    Ok((200, v))
}

/// The query string as urllib's parse_qs reads it: `+` is a space, percent-escapes decoded, blank
/// values dropped, the first of repeated names wins.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Query(pub Vec<(String, String)>);

fn percent_decode(s: &str) -> String {
    let b = s.as_bytes();
    let mut out = Vec::with_capacity(b.len());
    let mut i = 0;
    while i < b.len() {
        let hex = (b[i] == b'%' && i + 2 < b.len())
            .then(|| std::str::from_utf8(&b[i + 1..i + 3]).ok().and_then(|h| u8::from_str_radix(h, 16).ok()))
            .flatten();
        match (b[i], hex) {
            (_, Some(v)) => { out.push(v); i += 2; }
            (b'+', _) => out.push(b' '),
            (c, _) => out.push(c),
        }
        i += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

impl Query {
    pub fn parse(qs: &str) -> Query {
        Query(qs.split('&').filter_map(|p| {
            let (k, v) = p.split_once('=')?;
            let v = percent_decode(v);
            (!v.is_empty()).then(|| (percent_decode(k), v))
        }).collect())
    }

    pub fn get(&self, name: &str) -> Option<&str> {
        self.0.iter().find(|(k, _)| k == name).map(|(_, v)| v.as_str())
    }

    pub fn str_or<'a>(&'a self, name: &str, default: &'a str) -> &'a str {
        self.get(name).unwrap_or(default)
    }

    /// `int(q.get(name, [default])[0])`.
    pub fn int(&self, name: &str, default: i64) -> Result<i64, String> {
        match self.get(name) {
            None => Ok(default),
            Some(v) => v.trim().parse().map_err(|_| format!("invalid literal for int() with base 10: {}", py_repr(&json!(v)))),
        }
    }
}

/// When the content list was last fetched from the content server (`content update`).
static CONTENT_UPDATED: std::sync::Mutex<Option<std::time::Instant>> = std::sync::Mutex::new(None);

/// One `content state` line, "10884238, AI, Not selected, AdmiralAI, 4C4D4441, <md5>", as JSON; the
/// name may hold commas, so it is everything between the state and the two trailing ids.
fn content_line(line: &str) -> Option<Value> {
    let text = line.rsplit("] ").next().unwrap_or(line).trim();
    let f: Vec<&str> = text.split(", ").collect();
    if f.len() < 6 { return None; }
    let id: u64 = f[0].trim().parse().ok()?;
    Some(json!({"id": id, "type": f[1], "state": f[2], "name": f[3..f.len() - 2].join(", ")}))
}

/// GET /api/content?q=<text>: what the content server (BaNaNaS) offers whose name matches, from the
/// game's own `content state` (the list is fetched with `content update` at most every 10 minutes).
fn h_content(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let q = i.query.get("q").unwrap_or("").to_string();
    if q.trim().is_empty() || q.contains('\n') || q.contains('"') {
        return Ok((400, json!({"error": "?q= names what to look for, one line, e.g. AdmiralAI or AI"})));
    }
    let stale = CONTENT_UPDATED.lock().unwrap().map_or(true, |t| t.elapsed() > Duration::from_secs(600));
    if stale {
        g.console("content update")?;
        thread::sleep(Duration::from_secs(6));
        *CONTENT_UPDATED.lock().unwrap() = Some(std::time::Instant::now());
    }
    let mark = g.log_seq();
    g.console(&format!("content state {}", q.trim()))?;
    thread::sleep(Duration::from_secs(2));
    let items: Vec<Value> = g.log_after(mark).iter().filter_map(|l| content_line(l)).collect();
    ok(json!({"query": q.trim(), "items": items, "count": items.len()}))
}

/// POST /api/content/download {ids}: the owner's download of those content items (and the libraries
/// they need, which the game selects itself): `content select <id>` for each, then `content
/// download`. Answers the console's output and the AIs the game offers afterwards.
fn h_content_download(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let ids: Vec<u64> = i.body.get("ids").and_then(Value::as_array).into_iter().flatten().filter_map(Value::as_u64).collect();
    if ids.is_empty() {
        return Ok((400, json!({"error": "ids: the content ids to download, from GET /api/content"})));
    }
    let mark = g.log_seq();
    for id in &ids { g.console(&format!("content select {id}"))?; }
    g.console("content download")?;
    g.event("operator", &format!("content download of {} item(s): {:?}", ids.len(), ids));
    thread::sleep(Duration::from_secs_f64(py_float(i.body.get("wait"), 20.0)?.clamp(1.0, 300.0)));
    let output = g.log_after(mark);
    // What the game offers now: the AI list rescans its folders on start_ai, so the names come from
    // the downloaded tars (content_download/ai) as the console reports them.
    ok(json!({"ids": ids, "output": output}))
}

fn h_console(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let cmd = py_str(i.body.get("command").or(Some(&json!("")))).trim().to_string();
    if cmd.is_empty() || cmd.contains('\n') {
        return Ok((400, json!({"error": "one non-empty line"})));
    }
    if is_approval_line(&cmd) {
        // Approval is the owner's act: through /api/approve (the panel's Approve button), never a
        // console line any caller (an agent included) could send. Withdrawing (deny) stays open.
        g.event("operator", &format!("refused console line: {cmd}"));
        return Ok((403, json!({"error": "approvals go through POST /api/approve (the owner's button), not the console"})));
    }
    let mark = g.log_seq();
    g.console(&cmd)?;
    g.event("console", &cmd);
    thread::sleep(Duration::from_secs_f64(py_float(i.body.get("wait"), 0.6)?.clamp(0.0, 600.0)));
    ok(json!({"sent": cmd, "output": g.log_after(mark)}))
}

fn h_rhai(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let script = py_str(i.body.get("script").or(Some(&json!(""))));
    let timeout = py_float(i.body.get("timeout"), 120.0)?.max(0.0);
    let (tx, rx) = mpsc::channel();
    let abort = Arc::new(AtomicBool::new(false));
    let runner = spawn_script(g.panel_url(), script.clone(), abort.clone(), tx);
    let deadline = Instant::now() + Duration::from_secs_f64(timeout.min(86_400.0));
    let (mut out, mut err) = (Vec::new(), Vec::new());
    loop {
        match rx.recv_timeout(deadline.saturating_duration_since(Instant::now())) {
            Ok(ScriptLine::Out(l)) => out.push(l),
            Ok(ScriptLine::Err(l)) => err.push(l),
            Ok(ScriptLine::Exit(_)) | Err(mpsc::RecvTimeoutError::Disconnected) => break,
            Err(mpsc::RecvTimeoutError::Timeout) => {
                abort.store(true, Ordering::SeqCst);
                return Err(format!("the Rhai script ran longer than {timeout} seconds and was stopped"));
            }
        }
    }
    let code = runner.join().unwrap_or(1);
    g.event("rhai", &format!("{} -> exit {code}", head(&script, 80)));
    for l in &out { g.to_owner_console(&format!("[rhai] {l}"), "rhai"); }
    for l in &err { g.to_owner_console(&format!("[rhai] {l}"), "error"); }
    let mut text = out.join("\n");
    if !out.is_empty() { text.push('\n'); }
    text.push_str(&err.join("\n"));
    ok(json!({"exit": code, "output": text.trim_end()}))
}

fn h_auto(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let on = truthy(i.body.get("on"));
    g.auto_approve.store(on, Ordering::SeqCst);
    g.event("operator", &format!("auto-approve {}", if on { "ON" } else { "OFF" }));
    ok(json!({"auto_approve": on}))
}

fn h_start_ai(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let name = if truthy(i.body.get("ai")) { py_str(i.body.get("ai")) } else { g.args.ai.clone() };
    let settings = py_str(i.body.get("settings").or(Some(&json!("")))).trim().to_string();
    let cmd = format!("start_ai {name} {settings}").trim().to_string();
    g.console(&cmd)?;
    g.ai_started.store(true, Ordering::SeqCst);
    g.event("ai", &cmd);
    ok(json!({"ok": true}))
}

fn h_view_attach(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let f = PathBuf::from(py_str(i.body.get("token_file").or(Some(&json!("")))));
    if !f.is_file() {
        return Ok((400, json!({"error": format!("no token file at {}", f.display())})));
    }
    *g.view_token.lock().unwrap() = Some(f.display().to_string());
    registry_write(&format!("client-{}", g.args.panel_port), &json!({
        "kind": "client", "role": "GUI client (owner view)", "token_file": f.display().to_string(), "panel": g.panel_url()}));
    g.event("view", &format!("GUI client attached ({})", f.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default()));
    ok(json!({"attached": true, "state": g.view_call("openttd.view.state", json!({}))?}))
}

fn h_action_plan(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let Some(d) = std::env::var("MCP_ALGOGLYPH_DIR").ok().filter(|d| !d.is_empty()) else {
        return Ok((503, json!({"error": "no AlgoGlyph runner: set MCP_ALGOGLYPH_DIR (mcp-e2e finds one)"})));
    };
    let mut inp = i.body.as_object().cloned().unwrap_or_default();
    if !inp.contains_key("cash") || !inp.contains_key("loan") {
        // Money and loan from the console's `companies` line (openttd://companies has no loan on
        // every build): the requested company, else the first AI company.
        let mark = g.log_seq();
        g.console("companies")?;
        thread::sleep(Duration::from_millis(800));
        let rows: Vec<(i64, i64, i64)> = g.log_after(mark).iter().filter_map(|l| company_row(l)).collect();
        let want = inp.get("company").and_then(Value::as_i64);
        let row = rows.iter().find(|r| want.map_or(false, |w| r.0 - 1 == w)).or(rows.first());
        if let Some(r) = row {
            inp.entry("cash").or_insert(json!(r.1));
            inp.entry("loan").or_insert(json!(r.2));
        }
    }
    if !inp.contains_key("max_loan") {
        inp.insert("max_loan".into(), json!(g.max_loan().unwrap_or(300000)));
    }
    inp.entry("loan_step").or_insert(json!(10000));
    inp.remove("company");
    let inp = Value::Object(inp);
    // The answer goes to a file, not a pipe: the flow CLI ends with process.exit(), which drops what a
    // pipe has not taken yet (an answer above 64 KiB arrived cut off, 2026-11-16).
    let path = g.tmp.join(format!("actionplan.{}.{:?}.json", std::process::id(), thread::current().id()));
    let file = std::fs::File::create(&path).map_err(|e| format!("{}: {e}", path.display()))?;
    let r = run(Command::new("npm").args(["run", "--silent", "flow", "--", "kernel", "action-plan", "-"]).current_dir(&d),
                Some(inp.to_string().into_bytes()), Duration::from_secs(120), Some(file));
    let text = std::fs::read_to_string(&path).unwrap_or_default();
    let _ = std::fs::remove_file(&path);
    let r = r?;
    let Ok(Value::Object(mut out)) = serde_json::from_str::<Value>(&text) else {
        let msg = if r.stderr.is_empty() { text.trim().to_string() } else { r.stderr.trim().to_string() };
        return Ok((502, json!({"error": head(&msg, 400)})));
    };
    out.insert("input".into(), inp.clone());
    out.insert("runner".into(), json!(d));
    let ok_ = truthy(out.get("ok"));
    let n = inp["actions"].as_array().map_or(0, Vec::len);
    g.event("plan", &format!("action plan: {n} action(s) -> {}", if ok_ {
        format!("{} step(s)", out.get("result").and_then(|r| r.get("steps")).and_then(Value::as_array).map_or(0, Vec::len))
    } else {
        format!("refused: {}", py_str(out.get("errors")))
    }));
    Ok((if ok_ { 200 } else { 422 }, Value::Object(out)))
}

fn h_intent(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let text = py_str(i.body.get("text"));
    let exe = sibling("mcp-intent");
    if !exe.exists() {
        return Ok((503, json!({"error": BUILD_IT})));
    }
    // The planner's portfolio needs a budget to solve: the caller's, else what a company may borrow.
    let mut params = i.body.get("params").and_then(Value::as_object).cloned().unwrap_or_default();
    if !params.contains_key("budget") {
        if let Some(ml) = g.max_loan().filter(|m| *m != 0) { params.insert("budget".into(), json!(ml)); }
    }
    let mut cmd = Command::new(&exe);
    cmd.arg("--panel").arg(g.panel_url());
    if !params.is_empty() { cmd.arg("--params").arg(Value::Object(params).to_string()); }
    cmd.arg(&text);
    let r = run(&mut cmd, None, Duration::from_secs(180), None)?;
    if r.code != 0 {
        return Ok((if r.code == 2 { 400 } else { 502 }, json!({"error": r.stderr.trim()})));
    }
    let out: Value = serde_json::from_str(&r.stdout).map_err(|e| e.to_string())?;
    let names: Vec<Value> = out["intent"]["subjects"].as_array().into_iter().flatten().map(|x| x["name"].clone()).collect();
    g.event("intent", &format!("{} -> {} {} {}: {} candidate(s)", py_repr(&json!(text)), py_str(out["intent"].get("action")),
                              py_str(out["intent"].get("ordering")), py_repr(&Value::Array(names)),
                              out["answer"]["candidates"].as_array().map_or(0, Vec::len)));
    ok(out)
}

/// The calibration store, research/calibration.json (src/calibrate.rs `record`).
fn calibration_file() -> std::path::PathBuf {
    crate::panel_results::repo().join("research/calibration.json")
}

fn h_calibration(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    Ok(match std::fs::read_to_string(calibration_file()) {
        Ok(t) => (200, serde_json::from_str(&t).map_err(|e| e.to_string())?),
        Err(_) => (200, json!({"readings": [], "params": {"calibration": {}}, "note": "no reading recorded yet: POST /api/calibration"})),
    })
}

/// Measure the running game's lines against the plan and add the reading to the store.
fn h_calibration_record(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let exe = sibling("mcp-plan");
    if !exe.exists() {
        return Ok((503, json!({"error": BUILD_IT})));
    }
    let file = calibration_file();
    let mut cmd = Command::new(&exe);
    cmd.arg("--panel").arg(g.panel_url()).arg("--limit").arg("500").arg("--calibrate").arg("--record").arg(&file);
    let r = run(&mut cmd, None, Duration::from_secs(180), None)?;
    if r.code != 0 {
        return Ok((502, json!({"error": r.stderr.trim()})));
    }
    let out: Value = serde_json::from_str(&r.stdout).map_err(|e| e.to_string())?;
    let rec = out.pointer("/calibration/recorded").cloned().unwrap_or(Value::Null);
    g.event("ai", &format!("calibration recorded in research/calibration.json: {} reading(s), {}", rec["readings"], rec["params"]));
    Ok((200, json!({"reading": out["calibration"], "recorded": rec})))
}

fn h_plan(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let exe = sibling("mcp-plan");
    if !exe.exists() {
        return Ok((503, json!({"error": BUILD_IT})));
    }
    let limit = i.query.int("limit", 20)?;
    let params = i.query.str_or("params", "");
    let track = i.query.int("track", 0)?;
    let yards = i.query.int("yards", 0)?;
    let calibrate = i.query.int("calibrate", 0)? != 0;
    let mut cmd = Command::new(&exe);
    // Calibration matches running lines to candidates, so it needs the long list, not the top few.
    let limit = if calibrate { limit.max(500) } else { limit };
    cmd.arg("--panel").arg(g.panel_url()).arg("--limit").arg(limit.clamp(1, 500).to_string());
    if calibrate { cmd.arg("--calibrate"); }
    if !params.is_empty() { cmd.arg("--params").arg(params); }
    if track > 0 {
        if std::env::var("MCP_ALGOGLYPH_DIR").map_or(true, |d| d.is_empty()) {
            return Ok((503, json!({"error": "track=N needs AlgoGlyph track-planning: MCP_ALGOGLYPH_DIR is not set"})));
        }
        cmd.arg("--track").arg(track.min(10).to_string());
        if yards > 0 { cmd.arg("--yards").arg(yards.min(6).to_string()); }
    }
    let r = run(&mut cmd, None, Duration::from_secs(60 + 60 * track.clamp(0, 10) as u64 + 60 * yards.clamp(0, 6) as u64), None)?;
    if r.code != 0 {
        return Ok((if r.code == 2 { 400 } else { 502 }, json!({"error": r.stderr.trim()})));
    }
    ok(serde_json::from_str(&r.stdout).map_err(|e| e.to_string())?)
}

/// What to build, decided by AlgoGlyph's transport-investment kernel over the planner's own
/// candidates (one kernel run per cargo, then the portfolio knapsack): mcp-plan --invest.
fn h_investment(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let exe = sibling("mcp-plan");
    if !exe.exists() {
        return Ok((503, json!({"error": BUILD_IT})));
    }
    if std::env::var("MCP_ALGOGLYPH_DIR").map_or(true, |d| d.is_empty()) {
        return Ok((503, json!({"error": "MCP_ALGOGLYPH_DIR is not set (mcp-e2e sets it when it finds an AlgoGlyph checkout)"})));
    }
    let params = i.query.str_or("params", "");
    let p: Value = if params.is_empty() { json!({}) } else {
        match serde_json::from_str(params) { Ok(v) => v, Err(e) => return Ok((400, json!({"error": format!("params is not JSON: {e}")}))) }
    };
    let Value::Object(mut p) = p else { return Ok((400, json!({"error": "params must be a JSON object"}))) };
    if !p.contains_key("budget") {
        if let Some(ml) = g.max_loan().filter(|m| *m != 0) { p.insert("budget".into(), json!(ml)); }
    }
    let r = run(Command::new(&exe).arg("--panel").arg(g.panel_url()).arg("--invest").arg("--params").arg(Value::Object(p).to_string()),
                None, Duration::from_secs(240), None)?;
    if r.code != 0 {
        return Ok((if r.code == 2 { 400 } else { 502 }, json!({"error": r.stderr.trim()})));
    }
    let out: Value = serde_json::from_str(&r.stdout).map_err(|e| e.to_string())?;
    let inv = out.get("investment").cloned().unwrap_or(json!({}));
    let chosen = inv.pointer("/portfolio/chosen").and_then(Value::as_array).cloned().unwrap_or_default();
    g.event("plan", &format!("investment: {} chosen of {} by {}: {}", chosen.len(), py_str(inv.get("candidates")),
                             py_str(inv.pointer("/portfolio/solver")),
                             chosen.iter().map(|c| format!("{} {} roi {:.2}", py_str(c.get("cargo")), py_str(c.get("mode")),
                                                           c.get("roi").and_then(Value::as_f64).unwrap_or(0.0)))
                                 .collect::<Vec<_>>().join(", ")));
    ok(json!({"investment": inv, "planner_portfolio": out.get("portfolio"), "params": out.get("params")}))
}

fn h_graph(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let exe = sibling("mcp-graph");
    if !exe.exists() {
        return Ok((503, json!({"error": BUILD_IT})));
    }
    let r = run(Command::new(&exe).arg("--panel").arg(g.panel_url()), None, Duration::from_secs(60), None)?;
    if r.code != 0 {
        return Ok((502, json!({"error": r.stderr.trim()})));
    }
    ok(serde_json::from_str(&r.stdout).map_err(|e| e.to_string())?)
}

fn h_status(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(json!({
        "server_running": g.server_running(), "mcp_up": g.mcp_up.load(Ordering::SeqCst), "endpoint": g.endpoint(),
        "ai": g.args.ai, "ai_started": g.ai_started.load(Ordering::SeqCst), "auto_approve": g.auto_approve.load(Ordering::SeqCst),
        "pending": g.pending_json(), "game_port": g.args.game_port, "events": g.events_json(), "event_seq": g.bus.seq()}))
}

fn h_events(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let since = i.query.int("since", 0)?.max(0) as u64;
    let kinds = kinds_of(i.query.str_or("kinds", ""));
    let limit = i.query.int("limit", 500)?.max(0) as usize;
    ok(json!({"events": g.bus.since(since, &kinds, limit), "last_seq": g.bus.seq()}))
}

fn h_resource(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(g.resource(i.query.str_or("uri", ""))?)
}

fn h_log(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let log = g.log.lock().unwrap();
    let skip = log.lines.len().saturating_sub(150);
    ok(json!({"lines": log.lines.iter().skip(skip).map(|(_, l)| l.clone()).collect::<Vec<_>>()}))
}

fn h_overview(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(json!({"state": g.resource("openttd://game/state")?, "companies": g.resource("openttd://companies")?,
              "decisions": g.resource("openttd://decisions")?}))
}

fn h_catalogue(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(json!({"resources": g.rpc("resources/list", None)?, "tools": g.rpc("tools/list", None)?,
              "initialize": g.rpc("initialize", Some(json!({"protocolVersion": "2025-11-25", "capabilities": {},
                                                             "clientInfo": {"name": "openttd-mcp-panel", "version": "1"}})))?}))
}

fn h_rpc(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(g.rpc(i.body["method"].as_str().unwrap_or(""), i.body.get("params").cloned())?)
}

fn h_pause(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(g.set_pause(i.body.get("pause").map_or(true, |v| truthy(Some(v))))?)
}

fn h_approve(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(g.approve(py_int(&i.body["id"])?)?)
}

fn h_bots(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(panel_bot::bots(g)?)
}

fn h_bot_choices(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(panel_bot::bot_choices(g)?)
}

fn h_bot_start(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(panel_bot::start_bot(g, &py_str(i.body.get("ai")), i.body.get("settings").cloned().unwrap_or(json!({})))?)
}

fn h_bot_setting(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(panel_bot::set_bot_setting(g, &i.body["company"], &py_str(i.body.get("name")), &i.body["value"])?)
}

fn h_bridge_companies(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(panel_decision::companies(g)?)
}

fn h_decision_pull(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    let url = match i.body.get("url").and_then(Value::as_str).filter(|u| !u.is_empty()) {
        Some(u) => u.to_string(),
        None => std::env::var("PRISMATIC_DECISION_URL").unwrap_or_default(),
    };
    if url.is_empty() {
        return Ok((501, json!({"error": "no decision URL: pass url, or set PRISMATIC_DECISION_URL (prismatic_api: http://127.0.0.1:4000/api/openttd/decision)"})));
    }
    panel_decision::pull(g, &url)
}

fn h_instances(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(json!({"registry": registry().display().to_string(), "instances": discover()}))
}

fn h_instances_call(_: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    instance_call(&py_str(i.body.get("name")), &py_str(i.body.get("method")), i.body.get("params").unwrap_or(&Value::Null))
}

fn h_news(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(g.view_call("openttd.view.news.list", json!({"limit": i.query.int("limit", 20)?}))?)
}

fn h_news_show(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(g.view_call("openttd.view.news.show", i.body.clone())?)
}

fn h_ui_classes(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    let src = here().join("../../../src/window_type.h");
    window_classes(&std::fs::read_to_string(&src).map_err(|e| format!("{}: {e}", src.display()))?).map(|v| (200, v))
}

fn h_ask(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(g.view_call("openttd.view.ask", i.body.clone())?)
}

fn h_ask_answers(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(g.ask_answers()?)
}

fn h_news_close(g: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> {
    ok(g.view_call("openttd.view.news.close", json!({}))?)
}

fn h_view_tool(g: &Arc<Game>, i: &Input) -> Result<(u16, Value), String> {
    ok(g.view_call(&py_str(i.body.get("name")), i.body.get("arguments").cloned().unwrap_or(json!({})))?)
}

/// The panel's own HTTP surface, declared once: the router dispatches from this table and
/// /openapi.json documents it. Routes derived from the live MCP catalogue are added at request time
/// (derive_routes), from the same tools/list and resources/list every other surface reads.
pub fn routes() -> Vec<Route> {
    let r = |method, path, op_id, summary, needs_mcp, schema, handler: Handler| Route { method, path, op_id, summary, needs_mcp, schema, handler };
    let st = || json!({"type": "string"});
    vec![
        r("GET", "/api/status", "status", "Panel, server and MCP status; pending approvals; recent events", false, None, h_status),
        r("GET", "/api/log", "log", "The server console log (token redacted)", false, None, h_log),
        r("GET", "/api/events", "events", "Events after ?since=<seq>, optionally ?kinds=game,company,... ; SSE: /api/events/stream", false, None, h_events),
        r("GET", "/api/overview", "overview", "Game state, companies and decisions in one read", true, None, h_overview),
        r("GET", "/api/results", "results", "Every duel result (research/results, written by mcp-duel), newest first: seed, months, label, opponent, winner and both companies' final value", false, None, panel_results::h_results),
        r("GET", "/api/result", "result", "One duel result by name (?name=duel-...): summary, month-by-month series, the plan, both bots' logs and the written reading", false, None, panel_results::h_result),
        r("POST", "/api/duel", "duelStart", "Start a duel in the background (tools/research/mcp-duel): the planned bot against an unplanned one or an opponent AI, on its own ports; one at a time", false,
          s(json!({"months": {"type": "integer", "minimum": 1, "maximum": 240}, "seed": {"type": "integer"}, "label": {"type": "string"}, "opponent": {"type": "string"}, "bot_settings": {"type": "string", "description": "extra settings for the planned bot, e.g. own_lines=8"}, "calibrated": {"type": "boolean", "description": "start the planned bot's plan from research/calibration.json (GET /api/calibration)"}}), &[]), panel_results::h_duel_start),
        r("GET", "/api/calibration", "calibration", "The calibration store (research/calibration.json): every recorded reading of real against predicted profit per vehicle, and params.calibration derived from all of them (per mode, the median over every line)", false, None, h_calibration),
        r("POST", "/api/calibration", "calibrationRecord", "Measure this game's running lines against the plan (mcp-plan --calibrate) and add the reading to research/calibration.json; the bot start uses the store when PRISMATIC_PLAN_CALIBRATION names it (a duel with calibrated: true). Commit stays the owner's", true,
          s(json!({}), &[]), h_calibration_record),
        r("GET", "/api/scoreboard", "scoreboard", "The README duel scoreboard as tools/research/readme-scoreboard derives it from research/results: best run, wins of all duels, the seed-31337 series, and whether README.md is current or stale", false, None, panel_results::h_scoreboard),
        r("POST", "/api/scoreboard", "scoreboardWrite", "Regenerate the scoreboard region of README.md from research/results (tools/research/readme-scoreboard); answers the new scoreboard. Commit and push stay the owner's", false,
          s(json!({}), &[]), panel_results::h_scoreboard_write),
        r("GET", "/api/duel", "duelStatus", "The duel started from the panel: running or finished, the last month line, the winner line and the result's name", false, None, panel_results::h_duel_status),
        r("GET", "/api/overview/history", "overviewHistory", "The overview's history kept by the panel: one sample per game day (tick, date, vehicles, towns, per company money, value, loan, net); ?since=<tick> for the newer ones", false, None, h_history),
        r("GET", "/api/catalogue", "catalogue", "MCP initialize, resources/list and tools/list", true, None, h_catalogue),
        r("GET", "/api/resource", "readResource", "Read one MCP resource by ?uri=", true, None, h_resource),
        r("POST", "/api/rpc", "rpc", "Raw MCP JSON-RPC with the panel's token", true,
          s(json!({"method": st(), "params": obj()}), &["method"]), h_rpc),
        r("POST", "/api/console", "console", "One OpenTTD console line as the operator; returns its output", false,
          s(json!({"command": st(), "wait": {"type": "number"}}), &["command"]), h_console),
        r("GET", "/api/content", "content", "What the content server (BaNaNaS) offers whose name matches ?q= (AIs, libraries, NewGRFs ...): id, type, state, name, from the game's own content state", false,
          None, h_content),
        r("POST", "/api/content/download", "contentDownload", "The owner's download of content items by id (an AI and the libraries it needs), through the game's content select and content download", false,
          s(json!({"ids": {"type": "array", "items": {"type": "integer"}}, "wait": {"type": "number"}}), &["ids"]), h_content_download),
        r("POST", "/api/pause", "pause", "Pause or resume through the approval-gated MCP tool", true,
          s(json!({"pause": {"type": "boolean"}}), &["pause"]), h_pause),
        r("POST", "/api/approve", "approve", "The operator's approval of a pending decision, then its resubmission", true,
          s(json!({"id": {"type": "integer"}}), &["id"]), h_approve),
        r("POST", "/api/auto-approve", "autoApprove", "The owner's full-auto switch", false,
          s(json!({"on": {"type": "boolean"}}), &["on"]), h_auto),
        r("POST", "/api/start-ai", "startAi", "Start a company AI with an optional settings string", false,
          s(json!({"ai": st(), "settings": st()}), &[]), h_start_ai),
        r("GET", "/api/bots", "bots", "The AIs the owner can run as their bot, their settings (from openttd.ai.list, else info.nut) and the running AI companies", true,
          None, h_bots),
        r("GET", "/api/bots/choices", "botChoices", "What a bot setting naming a town, industry or cargo may name, from openttd://world/graph (towns by population, industries with their cargos, every cargo)", true,
          None, h_bot_choices),
        r("POST", "/api/bots/start", "botStart", "Start the owner's bot: an AI with a settings map", false,
          s(json!({"ai": st(), "settings": obj()}), &["ai"]), h_bot_start),
        r("POST", "/api/bots/setting", "botSetting", "Change one setting of a running bot (openttd.ai.set_setting, approval-gated)", true,
          s(json!({"company": {"type": "integer"}, "name": st(), "value": {"type": "integer"}}), &["company", "name", "value"]), h_bot_setting),
        r("GET", "/api/bridge/companies", "bridgeCompanies", "Every company as the bridge's GET /companies answers it (CompanyOut: money, loan, income from the vehicles' profit, company value, vehicle counts), so Prismatic decides from this game with OPENTTD_BRIDGE_URL=<panel>/api/bridge (src/panel_decision.rs)", true,
          None, h_bridge_companies),
        r("POST", "/api/decision/pull", "decisionPull", "Prismatic decides the bot: GET a decision directive {ai, company_id, settings} from url (default PRISMATIC_DECISION_URL), check it against research/prismatic-bridge/decision-contract.schema.json and apply it (settings on the running AI through openttd.ai.set_setting, approval-gated; else start_ai), answering a DecisionPullResult", true,
          s(json!({"url": st()}), &[]), h_decision_pull),
        r("GET", "/api/plan", "plan", "A route plan: town pairs ranked by model v2 from the game's payment formula, vehicle speeds and base prices (tools/research/mcp-rhai src/plan.rs), each with its mode of transport (bus, rail, air), plus the best pairs of each mode; ?limit=N&params=JSON overrides any assumption (the answer echoes the effective params); &track=N prices the N best rail lines of each list on the real map with AlgoGlyph track-planning (line_costs) before the portfolio is chosen; &calibrate=1 adds `calibration`: every running line's real profit per vehicle (openttd://vehicles profit_last_year) against this plan's prediction for the same towns or industries, the median ratio per mode, and `params.calibration` to pass back as params (src/calibrate.rs); with track, &yards=N proves up to N transfer yards offline (yards: can the other line still leave from a platform beside the better line's, its tiles blocked; src/yards.rs)", true,
          None, h_plan),
        r("GET", "/api/investment", "investment", "What to build, decided by AlgoGlyph's transport-investment kernel (routes of one or two legs through hubs, ranked by return on investment) over the planner's candidates from the live game, one run per cargo with that cargo's own vehicles, then one portfolio within the budget (default: the loan limit); ?params=JSON as /api/plan. The planner's own portfolio comes alongside for comparison", true,
          None, h_investment),
        r("POST", "/api/actionplan", "actionPlan", "Plan before acting: a set of actions with dependencies ({id, kind, label?, cost?, income?, amount?+step?, repeat?, after?}) turned by AlgoGlyph's action-plan kernel into a validated DAG and executable presses (repay 50k = 5 x repay 10k as one plan) with the cash and loan after each, refused if it would overdraw; cash, loan and loan limits default to the bot company's and the game's", true,
          s(json!({"actions": {"type": "array"}, "cash": {"type": "number"}, "loan": {"type": "number"}, "max_loan": {"type": "number"},
                   "loan_step": {"type": "number"}, "company": {"type": "integer"}}), &["actions"]), h_action_plan),
        r("POST", "/api/intent", "intent", "A sentence from the owner (\"najdi nejbližší les + pilu\", \"vybuduj nejlukrativnější trať\", \"find the most productive coal mine\") answered over the live game: nouns from the game's own language files and world graph, orderings nearest / most productive / most profitable, candidates in order, and for build the bot settings that carry out the first (tools/research/mcp-rhai src/intent.rs)", true,
          s(json!({"text": st(), "params": obj()}), &["text"]), h_intent),
        r("GET", "/api/graph", "graph", "The road network as a graph (nodes: junctions, ends, stations, town anchors; edges with length; components), derived from openttd://map, towns and stations by tools/research/mcp-rhai mcp-graph", true,
          None, h_graph),
        r("GET", "/api/instances", "instances", "Every OpenTTD MCP instance in the cluster (panels and GUI clients) from the shared registry, each checked with a live probe", true,
          None, h_instances),
        r("POST", "/api/instances/call", "instancesCall", "One MCP JSON-RPC call on a named instance of the cluster (a panel's server or a GUI client)", true,
          s(json!({"name": st(), "method": st(), "params": obj()}), &["name", "method"]), h_instances_call),
        r("GET", "/api/news", "news", "The news the owner's game window has shown, newest first (openttd.view.news.list on the GUI client); ?limit=N", true,
          None, h_news),
        r("POST", "/api/news", "newsShow", "Show a news message in the owner's game window, from the bot, a script or another AI (openttd.view.news.show): text, optional style normal|thin|small|company, a tile, x/y, town or vehicle for its viewport, or a company (and title) for the manager's face", true,
          s(json!({"text": st(), "style": {"type": "string", "enum": ["normal", "thin", "small", "company"]}, "company": {"type": "integer"},
                   "title": {"type": "string", "enum": ["none", "launch", "trouble", "merger", "bankrupt"]}, "tile": {"type": "integer"},
                   "x": {"type": "integer"}, "y": {"type": "integer"}, "town": {"type": "integer"}, "vehicle": {"type": "integer"}}), &["text"]), h_news_show),
        r("GET", "/api/ui/classes", "uiClasses", "Window class numbers to names, derived from src/window_type.h (for openttd.ui.windows)", false,
          None, h_ui_classes),
        r("POST", "/api/ask", "ask", "Ask the player a yes/no question in the game window (openttd.view.ask); the answer arrives as event ask.answered and in GET /api/ask: yes, no (also a closed dialog), or replaced by a newer question", true,
          s(json!({"question": st(), "caption": st()}), &["question"]), h_ask),
        r("GET", "/api/ask", "askAnswers", "Every question asked in the game window, newest first, with its answer (yes, no, replaced, or null while open)", true,
          None, h_ask_answers),
        r("POST", "/api/news/close", "newsClose", "Close the news window in the owner's game", true, None, h_news_close),
        r("POST", "/api/view/attach", "viewAttach", "Attach the GUI client the owner watches, by its MCP token file", false,
          s(json!({"token_file": st()}), &["token_file"]), h_view_attach),
        r("POST", "/api/view/tool", "viewTool", "Call a tool on the attached GUI client's own MCP server (openttd.view.* and the rest)", false,
          s(json!({"name": st(), "arguments": obj()}), &["name"]), h_view_tool),
        r("POST", "/api/rhai", "rhai", "Run a Rhai script against this panel, in the panel's own process (tools/research/mcp-rhai)", false,
          s(json!({"script": st(), "timeout": {"type": "number"}}), &["script"]), h_rhai),
    ]
}

// ---------------------------------------------------------------------- derived routes and OpenAPI

/// A route derived from a live MCP catalogue.
#[derive(Clone, Debug, PartialEq)]
pub struct Derived {
    pub method: &'static str,
    pub path: String,
    pub op_id: String,
    pub summary: String,
    pub schema: Option<Value>,
    pub item: Value,
}

/// `re.sub(r"\W", "_", name)`.
pub fn word(name: &str) -> String {
    name.chars().map(|c| if c.is_alphanumeric() || c == '_' { c } else { '_' }).collect()
}

/// One REST route per MCP tool (POST /api/tools/<name>, the tool's inputSchema as the body), per tool
/// of the attached GUI client (POST /api/client/<name>) and per MCP resource (GET
/// /api/resources/<path of openttd://...>), from the live catalogues.
pub fn derive_routes(tools: &[Value], client_tools: &[Value], resources: &[Value]) -> Vec<Derived> {
    let schema_of = |t: &Value| Some(t.get("inputSchema").filter(|s| truthy(Some(s))).cloned().unwrap_or_else(obj));
    let desc = |t: &Value, name: &str| t.get("description").and_then(Value::as_str).unwrap_or(name).to_string();
    let mut out = Vec::new();
    for t in tools {
        let Some(name) = t["name"].as_str() else { continue };
        out.push(Derived { method: "POST", path: format!("/api/tools/{name}"), op_id: format!("tool_{}", word(name)),
                           summary: desc(t, name), schema: schema_of(t), item: t.clone() });
    }
    // The attached GUI client's own tools (openttd.ui.*, openttd.input.*, openttd.view.* and whatever it
    // gains), one route each, from its live tools/list: the owner's window is a surface like the server.
    for t in client_tools {
        let Some(name) = t["name"].as_str() else { continue };
        out.push(Derived { method: "POST", path: format!("/api/client/{name}"), op_id: format!("client_{}", word(name)),
                           summary: format!("GUI client: {}", desc(t, name)), schema: schema_of(t), item: t.clone() });
    }
    for r in resources {
        let Some(uri) = r["uri"].as_str() else { continue };
        let path = uri.split_once("://").map_or(uri, |(_, p)| p);
        let summary = [r.get("description"), r.get("name")].into_iter().flatten().find(|v| truthy(Some(v)))
            .and_then(Value::as_str).unwrap_or(uri).to_string();
        out.push(Derived { method: "GET", path: format!("/api/resources/{path}"), op_id: format!("resource_{}", word(path)),
                           summary, schema: None, item: r.clone() });
    }
    out
}

/// The query parameters a declared GET route documents.
fn query_params(path: &str) -> &'static [&'static str] {
    match path {
        "/api/events" => &["since", "kinds", "limit"],
        "/api/resource" => &["uri"],
        "/api/news" => &["limit"],
        "/api/plan" => &["limit", "params", "track", "yards", "calibrate"],
        "/api/investment" => &["params"],
        "/api/content" => &["q"],
        "/api/result" => &["name"],
        _ => &[],
    }
}

/// The OpenAPI 3.1 document of the declared routes, the streaming routes, and the derived routes.
pub fn openapi_doc(routes: &[Route], derived: &[Derived], panel_port: u16) -> Value {
    let mut paths = Map::new();
    let mut op = |method: &str, path: &str, op_id: &str, summary: &str, schema: &Option<Value>, tag: &str, item: Option<&Value>| {
        let mut o = json!({"operationId": op_id, "summary": summary, "tags": [tag],
                           "responses": {"200": {"description": "OK", "content": {"application/json": {"schema": obj()}}}}});
        if let Some(sc) = schema {
            o["requestBody"] = json!({"required": true, "content": {"application/json": {"schema": sc}}});
        }
        let q = if method == "GET" { query_params(path) } else { &[] };
        if !q.is_empty() {
            o["parameters"] = Value::Array(q.iter().map(|n| json!({"name": n, "in": "query", "schema": {"type": "string"}})).collect());
        }
        if let Some(a) = item.and_then(|i| i.get("annotations")).filter(|a| truthy(Some(a))) {
            o["x-mcp-annotations"] = a.clone();
        }
        let entry = paths.entry(path.to_string()).or_insert_with(|| json!({}));
        entry[method.to_ascii_lowercase()] = o;
    };
    for r in routes {
        op(r.method, r.path, r.op_id, r.summary, &r.schema, "panel", None);
    }
    for d in derived {
        let tag = if d.path.starts_with("/api/client/") { "gui client tools" } else if d.method == "POST" { "mcp tools" } else { "mcp resources" };
        op(d.method, &d.path, &d.op_id, &d.summary, &d.schema, tag, Some(&d.item));
    }
    paths.insert("/api/rhai/stream".into(), json!({"post": {"operationId": "rhaiStream", "tags": ["panel"],
        "summary": "Run a Rhai script in the panel's process and stream its output line by line (SSE: out, err, exit)",
        "requestBody": {"required": true, "content": {"application/json": {"schema": {
            "type": "object", "properties": {"script": {"type": "string"}}, "required": ["script"]}}}},
        "responses": {"200": {"description": "text/event-stream"}}}}));
    paths.insert("/api/view/screenshot".into(), json!({"get": {"operationId": "viewScreenshot", "tags": ["view"],
        "summary": "What the attached GUI client shows right now, as PNG", "responses": {"200": {"description": "image/png"}}}}));
    paths.insert("/api/events/stream".into(), json!({"get": {"operationId": "eventStream", "tags": ["events"],
        "summary": "Server-sent events: every event as it is published (?since=, ?kinds=)",
        "responses": {"200": {"description": "text/event-stream"}}}}));
    json!({"openapi": "3.1.0",
           "info": {"title": "OpenTTD MCP panel (RESEARCH-ONLY)", "version": "1",
                    "description": "Declared once in tools/research/mcp-rhai/src/panel.rs (routes()) and derived from the live \
                                    MCP catalogues: every MCP tool and resource of the running game, and every tool \
                                    of the attached GUI client (/api/client/...), is a route here."},
           "servers": [{"url": format!("http://127.0.0.1:{panel_port}")}], "paths": paths})
}

/// The derived routes of the running game, or none when the catalogue cannot be read.
fn live_derived(g: &Game) -> Vec<Derived> {
    if !g.mcp_up.load(Ordering::SeqCst) {
        return vec![];
    }
    let list = |method: &str, key: &str| -> Result<Vec<Value>, String> {
        Ok(g.rpc(method, None)?.get("result").and_then(|r| r.get(key)).and_then(Value::as_array).cloned().unwrap_or_default())
    };
    match (list("tools/list", "tools"), list("resources/list", "resources")) {
        (Ok(t), Ok(r)) => derive_routes(&t, &g.client_tools(), &r),
        _ => vec![],
    }
}

/// What is wrong with a request body against its declared JSON schema: not an object, a required
/// field missing, a field of the wrong type, a value outside its enum. [] when it fits. One check for
/// every POST route, declared or derived, so a bad call answers 400 and never reaches a handler.
pub fn body_errors(schema: Option<&Value>, body: &Value) -> Vec<String> {
    let Some(schema) = schema.filter(|s| truthy(Some(s))) else { return vec![] };
    let Some(b) = body.as_object() else { return vec!["the body must be a JSON object".into()] };
    let mut errs: Vec<String> = schema["required"].as_array().into_iter().flatten().filter_map(Value::as_str)
        .filter(|k| !b.contains_key(*k)).map(|k| format!("missing {k}")).collect();
    for (k, spec) in schema["properties"].as_object().into_iter().flatten() {
        let (Some(v), true) = (b.get(k), spec.is_object()) else { continue };
        let want = spec.get("type").and_then(Value::as_str);
        let fits = match want {
            Some("string") => Some(v.is_string()),
            Some("integer") => Some(v.is_i64() || v.is_u64()),
            Some("number") => Some(v.is_number()),
            Some("boolean") => Some(v.is_boolean()),
            Some("object") => Some(v.is_object()),
            Some("array") => Some(v.is_array()),
            _ => None,
        };
        if fits == Some(false) {
            errs.push(format!("{k} must be {}", want.unwrap_or("")));
        } else if let Some(e) = spec.get("enum").and_then(Value::as_array) {
            if !e.contains(v) {
                errs.push(format!("{k} must be one of {}", py_repr(&Value::Array(e.clone()))));
            }
        }
    }
    errs
}

// ---------------------------------------------------------------------- Rhai in process

pub enum ScriptLine {
    Out(String),
    Err(String),
    Exit(i32),
}

/// Run a script in this process, in the engine mcp-rhai runs it in (functions derived from the panel
/// at `base`, the hub from MCP_HUB), every printed line sent as it is printed, the result value as a
/// last `Out` line, an error as `Err("mcp-rhai: ...")`, then `Exit` with mcp-rhai's exit status.
/// Setting `abort` stops the script at its next operation.
pub fn spawn_script(base: String, script: String, abort: Arc<AtomicBool>, tx: mpsc::Sender<ScriptLine>) -> thread::JoinHandle<i32> {
    thread::spawn(move || {
        let hub = std::env::var("MCP_HUB").ok().filter(|h| !h.is_empty());
        let mut engine = rhai_host::host(&base, hub.as_deref());
        let out = tx.clone();
        engine.on_print(move |s| for l in s.split('\n') { let _ = out.send(ScriptLine::Out(l.to_string())); });
        let out = tx.clone();
        engine.on_debug(move |s, src, pos| {
            let line = match src { Some(src) => format!("{src} @ {pos:?} | {s}"), None => format!("{pos:?} | {s}") };
            let _ = out.send(ScriptLine::Out(line));
        });
        let stop = abort.clone();
        engine.on_progress(move |_| stop.load(Ordering::SeqCst).then(|| rhai::Dynamic::from("stopped")));
        let code = match engine.eval::<rhai::Dynamic>(&script) {
            Ok(v) => {
                if !v.is_unit() {
                    for l in v.to_string().split('\n') { let _ = tx.send(ScriptLine::Out(l.to_string())); }
                }
                0
            }
            Err(e) => {
                let _ = tx.send(ScriptLine::Err(format!("mcp-rhai: {e}")));
                1
            }
        };
        let _ = tx.send(ScriptLine::Exit(code));
        code
    })
}

// ---------------------------------------------------------------------- HTTP

pub struct Request {
    pub method: String,
    pub path: String,
    pub query: Query,
    pub headers: Vec<(String, String)>,
    pub body: Vec<u8>,
}

impl Request {
    pub fn header(&self, name: &str) -> Option<&str> {
        self.headers.iter().find(|(k, _)| k.eq_ignore_ascii_case(name)).map(|(_, v)| v.as_str())
    }
}

/// One HTTP/1.1 request from a socket: the request line, headers and a Content-Length body.
pub fn read_request(stream: &mut TcpStream) -> Option<Request> {
    let _ = stream.set_read_timeout(Some(Duration::from_secs(30)));
    let mut buf = Vec::new();
    let mut chunk = [0u8; 8192];
    let end = loop {
        let n = stream.read(&mut chunk).ok()?;
        if n == 0 { return None; }
        buf.extend_from_slice(&chunk[..n]);
        if let Some(p) = buf.windows(4).position(|w| w == b"\r\n\r\n") { break p + 4; }
        if buf.len() > 1 << 16 { return None; }
    };
    let head = String::from_utf8_lossy(&buf[..end]).into_owned();
    let mut lines = head.split("\r\n");
    let mut first = lines.next()?.split_whitespace();
    let (method, target) = (first.next()?.to_string(), first.next()?.to_string());
    let headers: Vec<(String, String)> = lines.filter_map(|l| l.split_once(':').map(|(k, v)| (k.trim().to_string(), v.trim().to_string()))).collect();
    let length: usize = headers.iter().find(|(k, _)| k.eq_ignore_ascii_case("content-length"))
        .and_then(|(_, v)| v.parse().ok()).unwrap_or(0).min(64 << 20);
    while buf.len() < end + length {
        let n = stream.read(&mut chunk).ok()?;
        if n == 0 { break; }
        buf.extend_from_slice(&chunk[..n]);
    }
    let body = buf[end..buf.len().min(end + length)].to_vec();
    let (path, qs) = target.split_once('?').unwrap_or((&target, ""));
    Some(Request { method, path: path.to_string(), query: Query::parse(qs), headers, body })
}

fn send_json(stream: TcpStream, code: u16, v: &Value) {
    hub::respond(stream, code, "application/json", py_json(v).as_bytes());
}

fn sse_head(stream: &mut TcpStream) -> std::io::Result<()> {
    stream.write_all(b"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n")
}

/// Server-sent events from the bus: every event after ?since= (default: from now on) that passes
/// ?kinds=, a keep-alive comment every 15 s of silence.
fn stream_events(g: &Game, mut stream: TcpStream, q: &Query) {
    let mut since = match q.int("since", g.bus.seq() as i64) {
        Ok(s) => s.max(0) as u64,
        Err(e) => return send_json(stream, 502, &json!({"error": e})),
    };
    let kinds = kinds_of(q.str_or("kinds", ""));
    if sse_head(&mut stream).is_err() { return; }
    loop {
        g.bus.wait_past(since, Duration::from_secs(15));
        let (batch, next) = g.bus.stream_step(since, &kinds);
        since = next;
        let mut text = String::new();
        if batch.is_empty() { text.push_str(": keep-alive\n\n"); }
        for e in &batch {
            text.push_str(&format!("id: {}\nevent: {}\ndata: {}\n\n", e["seq"], e["kind"].as_str().unwrap_or(""), py_json(e)));
        }
        if stream.write_all(text.as_bytes()).and_then(|_| stream.flush()).is_err() { return; }
    }
}

/// Run a Rhai script and stream every line it prints as it is printed (SSE: event "out", "err", then
/// "exit" with the status). A closed connection stops the script.
fn stream_rhai(g: &Game, mut stream: TcpStream, body: &Value) {
    let script = py_str(body.get("script").or(Some(&json!(""))));
    if sse_head(&mut stream).is_err() { return; }
    let (tx, rx) = mpsc::channel();
    let abort = Arc::new(AtomicBool::new(false));
    let runner = spawn_script(g.panel_url(), script.clone(), abort.clone(), tx);
    g.event("rhai", &format!("{} (streamed)", head(&script, 80)));
    for line in rx {
        let (kind, data) = match line {
            ScriptLine::Out(l) => { g.to_owner_console(&format!("[rhai] {l}"), "rhai"); ("out", py_json(&json!(l))) }
            ScriptLine::Err(l) => { g.to_owner_console(&format!("[rhai] {l}"), "error"); ("err", py_json(&json!(l))) }
            ScriptLine::Exit(c) => ("exit", c.to_string()),
        };
        if stream.write_all(format!("event: {kind}\ndata: {data}\n\n").as_bytes()).and_then(|_| stream.flush()).is_err() {
            abort.store(true, Ordering::SeqCst);
            break;
        }
    }
    let _ = runner.join();
}

const SWAGGER_HTML: &str = r#"<!doctype html><html lang="en"><head><meta charset="utf-8"><title>OpenTTD MCP API</title>
<link rel="stylesheet" href="/vendor/swagger-ui/swagger-ui.css"></head><body>
<div id="swagger"></div><script src="/vendor/swagger-ui/swagger-ui-bundle.js"></script>
<script>SwaggerUIBundle({url: '/openapi.json', dom_id: '#swagger', deepLinking: true});</script></body></html>"#;

/// The front-end packages the pages load, from the panel's npm ci (node_modules).
fn vendor(path: &str) -> Option<(&'static str, &'static str)> {
    Some(match path {
        "/vendor/flowbite.min.js" => ("flowbite/dist/flowbite.min.js", "text/javascript"),
        "/vendor/echarts.min.js" => ("echarts/dist/echarts.min.js", "text/javascript"),
        "/vendor/swagger-ui/swagger-ui.css" => ("swagger-ui-dist/swagger-ui.css", "text/css"),
        "/vendor/swagger-ui/swagger-ui-bundle.js" => ("swagger-ui-dist/swagger-ui-bundle.js", "text/javascript"),
        _ => return None,
    })
}

/// What the attached GUI client shows right now, as PNG (openttd.view.screenshot).
fn screenshot(g: &Game) -> Result<Vec<u8>, String> {
    let shot = g.view_call("openttd.view.screenshot", json!({}))?;
    let f = PathBuf::from(shot["path"].as_str().ok_or_else(|| format!("no path in {}", py_json(&shot)))?);
    let mtime = |f: &Path| std::fs::metadata(f).and_then(|m| m.modified()).ok();
    let before = mtime(&f);
    // The client captures on its drawing thread; the file appears within a frame.
    for _ in 0..40 {
        if let Some(m) = mtime(&f) {
            if before.map_or(true, |b| m > b) { break; }
        }
        thread::sleep(Duration::from_millis(50));
    }
    std::fs::read(&f).map_err(|e| format!("{}: {e}", f.display()))
}

pub struct Panel {
    pub game: Arc<Game>,
    pub routes: Vec<Route>,
}

/// `&`, `<`, `>` and `"` as entities, for text put into a page.
fn html_escape(s: &str) -> String {
    s.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('"', "&quot;")
}

/// A page's title and description from the live catalogue, and whether the item exists:
/// /tools the catalogue itself, /tool/<name> a server tool, /client/<name> a GUI client tool,
/// /resource/<path> the resource openttd://<path>.
fn page_meta(g: &Arc<Game>, path: &str) -> (String, String, bool) {
    let find = |list: Vec<Value>, key: &str, want: &str| list.into_iter().find(|t| t[key] == json!(want))
        .map(|t| t["description"].as_str().unwrap_or("").to_string());
    if path == "/tools" {
        return ("MCP catalogue".into(), "Every tool, GUI client tool and resource of the game's MCP server, each with its own page".into(), true);
    }
    if let Some(name) = path.strip_prefix("/tool/") {
        let name = percent_decode(name);
        return match find(g.server_tools(), "name", &name) { Some(d) => (name, d, true), None => (format!("{name}: not in the catalogue"), String::new(), false) };
    }
    if let Some(name) = path.strip_prefix("/client/") {
        let name = percent_decode(name);
        return match find(g.client_tools(), "name", &name) { Some(d) => (name, d, true), None => (format!("{name}: no GUI client offers it"), String::new(), false) };
    }
    let rest = percent_decode(path.strip_prefix("/resource/").unwrap_or(""));
    let uri = format!("openttd://{rest}");
    let resources = g.rpc("resources/list", None).ok()
        .and_then(|r| r.pointer("/result/resources").and_then(Value::as_array).cloned()).unwrap_or_default();
    match find(resources, "uri", &uri) { Some(d) => (uri, d, true), None => (format!("{uri}: not in the catalogue"), String::new(), false) }
}

impl Panel {
    fn origin_ok(&self, req: &Request) -> bool {
        req.header("Origin").map_or(true, |o| o == self.game.panel_url())
    }

    pub fn handle(&self, mut stream: TcpStream) {
        let Some(req) = read_request(&mut stream) else { return };
        let g = &self.game;
        let file = |stream: TcpStream, f: PathBuf, ctype: &str| match std::fs::read(&f) {
            Ok(b) => hub::respond(stream, 200, ctype, &b),
            Err(_) => send_json(stream, 404, &json!({"error": "not found"})),
        };
        let body = match req.method.as_str() {
            "GET" => Value::Null,
            "POST" => {
                if !self.origin_ok(&req) {
                    return send_json(stream, 403, &json!({"error": "foreign origin"}));
                }
                let text = if req.body.is_empty() { &b"{}"[..] } else { &req.body[..] };
                match serde_json::from_slice::<Value>(text) {
                    Ok(v) => v,
                    Err(_) => return send_json(stream, 400, &json!({"error": "body is not JSON"})),
                }
            }
            _ => return send_json(stream, 501, &json!({"error": format!("unsupported method {}", req.method)})),
        };
        let path = req.path.as_str();
        if req.method == "POST" && path == "/api/rhai/stream" {
            if !body.is_object() { return send_json(stream, 400, &json!({"error": "invalid body: the body must be a JSON object"})); }
            return stream_rhai(g, stream, &body);
        }
        if req.method == "GET" {
            match path {
                "/" | "/index.html" => return file(stream, statics().join("index.html"), "text/html; charset=utf-8"),
                "/ui" => return file(stream, statics().join("ui.html"), "text/html; charset=utf-8"),
                "/world" => return file(stream, statics().join("world.html"), "text/html; charset=utf-8"),
                "/docs" => return hub::respond(stream, 200, "text/html; charset=utf-8", SWAGGER_HTML.as_bytes()),
                "/openapi.json" => return send_json(stream, 200, &openapi_doc(&self.routes, &live_derived(g), g.args.panel_port)),
                "/api/view/screenshot" => return match screenshot(g) {
                    Ok(png) => hub::respond(stream, 200, "image/png", &png),
                    Err(e) => send_json(stream, 503, &json!({"error": e})),
                },
                "/api/events/stream" => return stream_events(g, stream, &req.query),
                _ => {}
            }
            // One page per tool, GUI client tool and resource, and the catalogue of them: stable
            // addresses (/tool/<name>, /client/<name>, /resource/<path>, /tools) whose title and
            // description come from the live catalogue, so a link opens to its own page.
            // The duels: /results lists them, /result/<name> is one, titled from its own record.
            if path == "/results" || path.starts_with("/result/") {
                let (title, description, found) = panel_results::page_meta(path);
                return match std::fs::read_to_string(statics().join("results.html")) {
                    Ok(t) => {
                        let html = t.replace("__TITLE__", &html_escape(&title)).replace("__DESCRIPTION__", &html_escape(&description));
                        hub::respond(stream, if found { 200 } else { 404 }, "text/html; charset=utf-8", html.as_bytes())
                    }
                    Err(_) => send_json(stream, 404, &json!({"error": "static/results.html is missing"})),
                };
            }
            if path == "/tools" || path.starts_with("/tool/") || path.starts_with("/client/") || path.starts_with("/resource/") {
                let (title, description, found) = page_meta(g, path);
                return match std::fs::read_to_string(statics().join("tool.html")) {
                    Ok(t) => {
                        let html = t.replace("__TITLE__", &html_escape(&title)).replace("__DESCRIPTION__", &html_escape(&description));
                        hub::respond(stream, if found { 200 } else { 404 }, "text/html; charset=utf-8", html.as_bytes())
                    }
                    Err(_) => send_json(stream, 404, &json!({"error": "static/tool.html is missing"})),
                };
            }
            if let Some((rel, ctype)) = vendor(path) {
                return match std::fs::read(here().join("node_modules").join(rel)) {
                    Ok(b) => hub::respond(stream, 200, ctype, &b),
                    Err(_) => send_json(stream, 404, &json!({"error": "run npm ci in tools/research/mcp-panel"})),
                };
            }
            if let Some(rel) = path.strip_prefix("/static/") {
                let f = statics().join(rel);
                // Only files under static/: no parent steps, no absolute path.
                if !rel.split('/').any(|p| p == "..") && !rel.starts_with('/') && f.is_file() {
                    let ctype = match f.extension().and_then(|e| e.to_str()) { Some("css") => "text/css", Some("js") => "text/javascript", _ => "application/octet-stream" };
                    return file(stream, f, ctype);
                }
                return send_json(stream, 404, &json!({"error": "not found"}));
            }
        }
        let (code, out) = self.dispatch(&req, body);
        send_json(stream, code, &out);
    }

    /// The declared routes, then the derived ones: (status, JSON answer).
    pub fn dispatch(&self, req: &Request, body: Value) -> (u16, Value) {
        let g = &self.game;
        let (method, path) = (req.method.as_str(), req.path.as_str());
        let hit = self.routes.iter().find(|r| r.method == method && r.path == path);
        if method == "POST" {
            let mut schema = hit.and_then(|r| r.schema.clone());
            if hit.is_none() && (path.starts_with("/api/tools/") || path.starts_with("/api/client/")) {
                let name = path.splitn(4, '/').nth(3).unwrap_or("");
                let cat = if path.starts_with("/api/client/") { g.client_tools() } else { g.server_tools() };
                schema = cat.iter().find(|t| t["name"] == json!(name)).and_then(|t| t.get("inputSchema").cloned());
            }
            let errs = body_errors(schema.as_ref(), &body);
            if !errs.is_empty() {
                return (400, json!({"error": format!("invalid body: {}", errs.join("; "))}));
            }
        }
        let up = g.mcp_up.load(Ordering::SeqCst);
        let result: Result<(u16, Value), String> = (|| {
            if let Some(r) = hit {
                if r.needs_mcp && !up {
                    return Ok((503, json!({"error": "MCP server not up yet"})));
                }
                let input = Input { query: req.query.clone(), body: if method == "POST" { body.clone() } else { json!({}) } };
                return (r.handler)(g, &input);
            }
            if !up {
                return Ok((503, json!({"error": "MCP server not up yet"})));
            }
            if method == "POST" {
                if let Some(name) = path.strip_prefix("/api/client/") {
                    return Ok(match g.view_call(name, or_empty(&body)) {
                        Ok(v) => (200, v),
                        Err(e) => (503, json!({"error": e})),
                    });
                }
                if let Some(name) = path.strip_prefix("/api/tools/") {
                    let out = g.tool(name, or_empty(&body))?;
                    return Ok((rpc_status(&out), out));
                }
            }
            if method == "GET" {
                if let Some(rest) = path.strip_prefix("/api/resources/") {
                    let out = g.resource(&format!("openttd://{rest}"))?;
                    return Ok((rpc_status(&out), out));
                }
            }
            Ok((404, json!({"error": "not found"})))
        })();
        // Surface, never hide, a failing call. A route that needs the owner's window answers 503 on a
        // stack without one (headless, or the client restarting): the state is temporary, not a broken route.
        result.unwrap_or_else(|e| (if e.contains("no GUI client attached") { 503 } else { 502 }, json!({"error": e})))
    }
}

/// Serve the panel on `listener`, one thread per connection, until the process ends.
pub fn serve(listener: TcpListener, panel: Arc<Panel>) {
    for stream in listener.incoming().flatten() {
        let p = panel.clone();
        thread::spawn(move || p.handle(stream));
    }
}

/// Is something already listening on this loopback port? A second listener can bind the same
/// loopback port on macOS, and calls then reach either server at random (seen 2026-11-16).
pub fn port_busy(port: u16) -> bool {
    TcpStream::connect_timeout(&([127, 0, 0, 1], port).into(), Duration::from_millis(500)).is_ok()
}

pub fn free_port() -> Option<u16> {
    TcpListener::bind("127.0.0.1:0").ok()?.local_addr().ok().map(|a| a.port())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_failed_read_is_retried_once_and_a_command_only_when_it_never_left() {
        use ureq::ErrorKind::{ConnectionFailed, Io};
        assert!(rpc_retryable("resources/read", Io, 0));
        assert!(rpc_retryable("tools/list", Io, 0));
        assert!(!rpc_retryable("resources/read", Io, 1), "once");
        assert!(rpc_retryable("tools/call", ConnectionFailed, 0));
        assert!(!rpc_retryable("tools/call", Io, 0), "a command the server may have received is not sent twice");
    }

    #[test]
    fn the_history_keeps_one_sample_a_day_and_restarts_with_a_new_world() {
        let st = |t: i64, date: &str| json!({"game_tick": t, "game_date": date, "data": {"num_vehicles": 3, "num_towns": 9, "paused": false}});
        let cs = json!({"data": {"companies": [{"id": 0, "name": "A", "money": 500, "company_value": 900, "loan": 300}]}});
        let s0 = sample_of(&st(10, "1950-01-01"), &cs).unwrap();
        assert_eq!(s0["companies"][0]["net"], json!(200.0));
        assert_eq!(s0["vehicles"], json!(3), "an older server: the pool count");
        let newer = json!({"game_tick": 1, "game_date": "1950-01-01", "data": {"num_vehicles": 40, "fleet": {"train": 2, "road": 3, "ship": 0, "aircraft": 0, "total": 5}}});
        let s1 = sample_of(&newer, &cs).unwrap();
        assert_eq!(s1["vehicles"], json!(5), "the fleet, not the pool's wagons, parts and effects");
        assert_eq!(s1["fleet"]["road"], json!(3));
        assert!(sample_of(&json!({}), &cs).is_none());
        let mut h = VecDeque::new();
        push_sample(&mut h, s0);
        push_sample(&mut h, sample_of(&st(40, "1950-01-01"), &cs).unwrap());
        assert_eq!(h.len(), 1, "a second read of the same day replaces the first");
        assert_eq!(h[0]["t"], json!(40));
        push_sample(&mut h, sample_of(&st(80, "1950-01-02"), &cs).unwrap());
        assert_eq!(h.len(), 2);
        push_sample(&mut h, sample_of(&st(5, "1950-01-01"), &cs).unwrap());
        assert_eq!(h.len(), 1, "a lower tick is a new world");
        for d in 0..(HISTORY_MAX + 5) { push_sample(&mut h, sample_of(&st(100 + d as i64, &format!("d{d}")), &cs).unwrap()); }
        assert_eq!(h.len(), HISTORY_MAX);
    }

    #[test]
    fn page_paths_decode_and_page_text_is_escaped() {
        assert_eq!(percent_decode("openttd.ai.set_setting"), "openttd.ai.set_setting");
        assert_eq!(percent_decode("map%2Fways"), "map/ways");
        assert_eq!(percent_decode("trailing%2"), "trailing%2");
        assert_eq!(percent_decode("%C3%A9%2"), "é%2");
        assert_eq!(html_escape("<a href=\"x\">&</a>"), "&lt;a href=&quot;x&quot;&gt;&amp;&lt;/a&gt;");
    }

    #[test]
    fn a_content_state_line_is_parsed_with_names_that_hold_commas() {
        let a = content_line("10884238, AI, Not selected, AdmiralAI, 4C4D4441, 4CCD92FB8F8F01045145BE99A28E14A6").unwrap();
        assert_eq!(a, json!({"id": 10884238, "type": "AI", "state": "Not selected", "name": "AdmiralAI"}));
        let b = content_line("[2026-11-16 23:00:00] 11431842, AI, Selected, NoCAB, Bleeding Edge, 42414342, 7B5D").unwrap();
        assert_eq!(b["name"], "NoCAB, Bleeding Edge");
        assert!(content_line("id, type, state, name").is_none());
    }

    #[test]
    fn a_server_without_a_token_file_is_reached_through_the_panel_that_owns_it() {
        let recs = vec![
            json!({"kind": "panel", "name": "panel-1", "url": "http://127.0.0.1:1/", "mcp": "http://127.0.0.1:2/mcp"}),
            json!({"kind": "panel", "name": "panel-3", "url": "http://127.0.0.1:3", "mcp": "http://127.0.0.1:4/mcp"}),
        ];
        let owned = json!({"kind": "server", "endpoint": "http://127.0.0.1:2/mcp"});
        let stray = json!({"kind": "server", "endpoint": "http://127.0.0.1:9/mcp"});
        assert_eq!(owning_panel(&recs, &owned).as_deref(), Some("http://127.0.0.1:1"));
        assert_eq!(owning_panel(&recs, &stray), None);
        assert_eq!(owning_panel(&recs, &json!({"kind": "server"})), None);
    }

    fn no_op(_: &Arc<Game>, _: &Input) -> Result<(u16, Value), String> { Ok((200, json!({}))) }

    #[test]
    fn the_route_table_is_unique_and_each_post_but_close_declares_its_body() {
        let rs = routes();
        let mut seen = HashSet::new();
        for r in &rs {
            assert!(seen.insert((r.method, r.path)), "{} {} declared twice", r.method, r.path);
            assert!(r.path.starts_with("/api/"));
            assert!(!r.op_id.is_empty() && !r.summary.is_empty());
            if r.method == "POST" && r.path != "/api/news/close" {
                assert!(r.schema.is_some(), "{} has no body schema", r.path);
            }
            if r.method == "GET" { assert!(r.schema.is_none()); }
        }
        let ids: HashSet<&str> = rs.iter().map(|r| r.op_id).collect();
        assert_eq!(ids.len(), rs.len(), "operationIds are unique");
        // server.py's table, route for route, plus the routes added after the Python panel was
        // retired: GET /api/content, POST /api/content/download, GET /api/overview/history, and the
        // decision contract's GET /api/bridge/companies and POST /api/decision/pull, and the duels
        // (GET /api/results, GET /api/result, POST and GET /api/duel), and the README scoreboard
        // (GET and POST /api/scoreboard), and the calibration store (GET and POST /api/calibration).
        assert_eq!(rs.len(), 45);
        for (m, p) in [("GET", "/api/status"), ("POST", "/api/console"), ("POST", "/api/rhai"), ("GET", "/api/news"), ("POST", "/api/news"),
                       ("GET", "/api/content"), ("POST", "/api/content/download"),
                       ("GET", "/api/ask"), ("POST", "/api/ask"), ("POST", "/api/instances/call"), ("GET", "/api/ui/classes"),
                       ("GET", "/api/bridge/companies"), ("POST", "/api/decision/pull"),
                       ("GET", "/api/scoreboard"), ("POST", "/api/scoreboard")] {
            assert!(rs.iter().any(|r| r.method == m && r.path == p), "{m} {p}");
        }
        assert!(rs.iter().filter(|r| !r.needs_mcp).all(|r| ["/api/status", "/api/log", "/api/events", "/api/console", "/api/auto-approve",
            "/api/start-ai", "/api/bots/start", "/api/ui/classes", "/api/view/attach", "/api/view/tool", "/api/rhai",
            "/api/content", "/api/content/download", "/api/overview/history", "/api/results", "/api/result", "/api/duel", "/api/scoreboard", "/api/calibration"].contains(&r.path)));
    }

    #[test]
    fn openapi_is_derived_from_the_table_and_the_live_catalogue() {
        let tools = vec![json!({"name": "openttd.decision.validate", "description": "Validate", "inputSchema": {"type": "object", "properties": {"action": {"type": "object"}}},
                                "annotations": {"readOnlyHint": true}}),
                         json!({"name": "openttd.no_schema", "inputSchema": {}})];
        let client = vec![json!({"name": "openttd.view.state", "description": "View"})];
        let resources = vec![json!({"uri": "openttd://game/state", "name": "state"}), json!({"uri": "openttd://map", "description": "Map"})];
        let derived = derive_routes(&tools, &client, &resources);
        assert_eq!(derived.iter().map(|d| d.path.as_str()).collect::<Vec<_>>(),
                   ["/api/tools/openttd.decision.validate", "/api/tools/openttd.no_schema", "/api/client/openttd.view.state",
                    "/api/resources/game/state", "/api/resources/map"]);
        assert_eq!(derived[0].op_id, "tool_openttd_decision_validate");
        assert_eq!(derived[1].schema, Some(json!({"type": "object"})), "an empty inputSchema becomes a bare object");
        assert_eq!(derived[2].summary, "GUI client: View");
        assert_eq!(derived[3].summary, "state");
        let rs = routes();
        let doc = openapi_doc(&rs, &derived, 18800);
        assert_eq!(doc["openapi"], "3.1.0");
        assert_eq!(doc["servers"][0]["url"], "http://127.0.0.1:18800");
        let paths = doc["paths"].as_object().unwrap();
        for r in &rs {
            let op = &paths[r.path][r.method.to_ascii_lowercase()];
            assert_eq!(op["operationId"], r.op_id);
            assert_eq!(op["tags"], json!(["panel"]));
            assert_eq!(op.pointer("/requestBody/content/application~1json/schema"), r.schema.as_ref());
        }
        let v = &paths["/api/tools/openttd.decision.validate"]["post"];
        assert_eq!(v["requestBody"]["content"]["application/json"]["schema"], tools[0]["inputSchema"]);
        assert_eq!(v["x-mcp-annotations"], json!({"readOnlyHint": true}));
        assert_eq!(v["tags"], json!(["mcp tools"]));
        assert_eq!(paths["/api/client/openttd.view.state"]["post"]["tags"], json!(["gui client tools"]));
        assert_eq!(paths["/api/resources/map"]["get"]["tags"], json!(["mcp resources"]));
        assert!(paths["/api/resources/map"]["get"].get("requestBody").is_none());
        assert_eq!(paths["/api/events"]["get"]["parameters"].as_array().unwrap().len(), 3);
        assert_eq!(paths["/api/plan"]["get"]["parameters"][2]["name"], "track");
        for p in ["/api/rhai/stream", "/api/view/screenshot", "/api/events/stream"] { assert!(paths.contains_key(p)); }
        // GET and POST on one path are two operations of one path item.
        assert!(paths["/api/news"].get("get").is_some() && paths["/api/news"].get("post").is_some());
        assert_eq!(paths.len(), rs.iter().map(|r| r.path).collect::<HashSet<_>>().len() + 3 + derived.len());
        // The hub reads the same document into the same operations.
        let ops = hub::ops_from_openapi("panel-1", &doc);
        assert!(ops.iter().any(|o| o.id == "tool_openttd_decision_validate" && o.read_only));
        assert!(ops.iter().any(|o| o.id == "eventStream" && o.stream));
        let _ = no_op;
    }

    #[test]
    fn bodies_are_checked_against_their_schema() {
        let rs = routes();
        let schema = |p: &str| rs.iter().find(|r| r.method == "POST" && r.path == p).unwrap().schema.clone();
        let news = schema("/api/news");
        assert!(body_errors(news.as_ref(), &json!({"text": "x", "style": "thin", "x": 3})).is_empty());
        assert_eq!(body_errors(news.as_ref(), &json!({})), ["missing text"]);
        assert_eq!(body_errors(news.as_ref(), &json!({"text": 1})), ["text must be string"]);
        assert_eq!(body_errors(news.as_ref(), &json!({"text": "x", "style": "huge"})),
                   ["style must be one of ['normal', 'thin', 'small', 'company']"]);
        assert_eq!(body_errors(news.as_ref(), &json!({"text": "x", "x": true})), ["x must be integer"], "a boolean is no integer");
        assert_eq!(body_errors(news.as_ref(), &json!({"text": "x", "x": 1.5})), ["x must be integer"]);
        assert_eq!(body_errors(news.as_ref(), &json!([1])), ["the body must be a JSON object"]);
        let console = schema("/api/console");
        assert!(body_errors(console.as_ref(), &json!({"command": "date", "wait": 0})).is_empty());
        assert_eq!(body_errors(console.as_ref(), &json!({"command": "date", "wait": "1"})), ["wait must be number"]);
        assert!(body_errors(None, &json!([1])).is_empty(), "no schema, no check");
        assert!(body_errors(Some(&json!({})), &json!("x")).is_empty(), "an empty schema checks nothing");
        let derived = json!({"type": "object", "properties": {"pause": {"type": "boolean"}, "w": {"type": ["integer", "null"]}}, "required": ["pause"]});
        assert_eq!(body_errors(Some(&derived), &json!({"pause": 1, "w": "x"})), ["pause must be boolean"]);
    }

    #[test]
    fn the_bus_numbers_filters_and_streams_events() {
        let bus = Bus::new();
        assert_eq!(bus.seq(), 0);
        bus.publish("game.month", json!({"date": "1950-02-01"}));
        bus.publish("panel.console", json!({"text": "getdate"}));
        bus.publish("gamex", json!(null));
        bus.publish("company.founded", json!({"id": 1}));
        assert_eq!(bus.seq(), 4);
        let kinds = |v: Vec<Value>| v.iter().map(|e| e["kind"].as_str().unwrap().to_string()).collect::<Vec<_>>();
        assert_eq!(kinds(bus.since(0, &kinds_of("game"), 500)), ["game.month"], "a kind takes its children, not a longer name");
        assert_eq!(kinds(bus.since(0, &kinds_of("panel.console,company"), 500)), ["panel.console", "company.founded"]);
        assert_eq!(kinds(bus.since(2, &[], 500)), ["gamex", "company.founded"]);
        assert_eq!(bus.since(0, &[], 2).len(), 2);
        let e = &bus.since(0, &[], 1)[0];
        assert_eq!(e["seq"], 1);
        assert!(e["time"].as_str().unwrap().len() >= 19);
        assert_eq!(e["data"]["date"], "1950-02-01");
        let (batch, next) = bus.stream_step(1, &kinds_of("company"));
        assert_eq!((kinds(batch), next), (vec!["company.founded".to_string()], 4));
        let (batch, next) = bus.stream_step(4, &[]);
        assert!(batch.is_empty() && next == 4);
        // A waiting stream wakes when an event is published.
        let bus = Arc::new(bus);
        let b = bus.clone();
        let t = thread::spawn(move || b.wait_past(4, Duration::from_secs(5)));
        thread::sleep(Duration::from_millis(50));
        bus.publish("game.paused", json!({}));
        assert_eq!(t.join().unwrap(), 5);
        assert_eq!(bus.wait_past(5, Duration::from_millis(10)), 5, "nothing new: back after the timeout");
        for _ in 0..BUS_KEEP { bus.publish("x", json!(null)); }
        assert_eq!(bus.since(0, &[], usize::MAX).len(), BUS_KEEP, "the newest 5000 are kept");
    }

    #[test]
    fn game_events_are_derived_by_difference() {
        let snap = |tick: i64, date: &str, paused: bool, vehicles: i64, companies: Value, decisions: Value| Snapshot::of(
            &json!({"game_date": date, "game_tick": tick, "data": {"paused": paused, "num_vehicles": vehicles, "num_towns": 9}}),
            &json!({"data": {"companies": companies}}), &json!({"data": {"decisions": decisions}})).unwrap();
        let a = snap(100, "1950-01-31", false, 2, json!([{"id": 0, "name": "A", "money": 5}]), json!([{"decision_id": "d1", "status": "pending"}]));
        let first = derive_game_events(None, &a);
        assert_eq!(first.iter().map(|e| e.0).collect::<Vec<_>>(), ["game.observed", "company.observed"]);
        assert_eq!(first[0].1["companies"], 1);
        let b = snap(200, "1950-02-01", true, 3, json!([{"id": 0, "name": "B", "money": 7}, {"id": 1, "name": "C"}]),
                     json!([{"decision_id": "d1", "status": "approved"}, {"decision_id": "d2"}]));
        let ev = derive_game_events(Some(&a), &b);
        assert_eq!(ev.iter().map(|e| e.0).collect::<Vec<_>>(),
                   ["game.month", "game.paused", "vehicles.count", "company.founded", "company.renamed", "decision.updated", "decision.recorded"]);
        assert_eq!(ev[0].1["companies"]["0"]["money"], 7);
        assert_eq!(ev[4].1, json!({"id": 0, "from": "A", "to": "B"}));
        let c = snap(10, "1950-02-01", true, 3, json!([{"id": 1, "name": "C"}]), json!([{"decision_id": "d1", "status": "approved"}, {"decision_id": "d2"}]));
        assert_eq!(derive_game_events(Some(&b), &c).iter().map(|e| e.0).collect::<Vec<_>>(), ["game.new_world", "company.removed"]);
        assert!(derive_game_events(Some(&c), &c).is_empty());
        assert!(Snapshot::of(&json!({"error": "x"}), &json!({}), &json!({})).is_none());
    }

    #[test]
    fn console_lines_are_read_like_server_py_reads_them() {
        let tok = "0123456789abcdef0123456789ABCDEF00";
        let (line, found) = redact(&format!("[prismatic-mcp] Bearer token (keep it secret): {tok} end"), None);
        assert_eq!(line, "[prismatic-mcp] Bearer token (held by the panel): <REDACTED> end");
        assert_eq!(found.as_deref(), Some(tok));
        assert_eq!(redact(&format!("echo {tok}"), Some(tok)).0, "echo <REDACTED>");
        assert!(find_token("Bearer token (x): abc").is_none(), "too short for a token");
        assert_eq!(denied_id("denied #12 by the operator"), Some(12));
        assert_eq!(denied_id("denied #12x"), None);
        assert_eq!(denied_id("not denied #12"), None);
        assert_eq!(plan_request("dbg: [prismatic-plan] 61206 najdi les"), Some((61206, "najdi les".into())));
        assert_eq!(plan_request("[prismatic-plan] x [prismatic-plan] 7 b"), Some((7, "b".into())));
        assert_eq!(plan_request("[prismatic-plan] 7 "), None);
        assert_eq!(ai_log("[2026] dbg: [script:4] [1] [W] PrismaticAI: low cash"), Some((1, "warning".into(), "PrismaticAI: low cash".into())));
        assert_eq!(ai_log("[script:4] [1] [X] no"), None);
        assert_eq!(max_loan_of("Current value for 'difficulty.max_loan' is '300000' (min: 0)"), Some(300000));
        assert_eq!(max_loan_of("max_loan is: 5000"), Some(5000));
        assert_eq!(max_loan_of("max_loan unknown"), None);
        assert_eq!(company_row("#:1(Blue) Company Name: 'Prismatic & Co.'  Year Founded: 1950  Money: -5694  Loan: 100000  Value: 1"),
                   Some((1, -5694, 100000)));
        assert!(is_approval_line("prismatic_mcp approve 999999"));
        assert!(is_approval_line("  MCP   Auto-Approve on"));
        assert!(is_approval_line("mcp auto_approve"));
        assert!(!is_approval_line("prismatic_mcp approved"));
        assert!(!is_approval_line("prismatic_mcp status"));
        assert!(!is_approval_line("mcpapprove 1"));
        assert!(!is_approval_line("prismatic_mcp deny 3"));
    }

    #[test]
    fn json_is_written_as_python_writes_it() {
        assert_eq!(py_json(&json!({"mcp_up": true, "a": [1, 2.5, null], "s": "les + pilu"})),
                   r#"{"a": [1, 2.5, null], "mcp_up": true, "s": "les + pilu"}"#);
        // Non-ASCII as \uXXXX (ensure_ascii), astral characters as a surrogate pair.
        let bs = '\\';
        let text: String = [0xe9, 0x1f642].iter().filter_map(|c| char::from_u32(*c)).collect();
        assert_eq!(py_json(&json!(text)), format!("\"{bs}u00e9{bs}ud83d{bs}ude42\""));
        assert_eq!(py_repr(&json!(["Forest", "it's"])), r#"['Forest', "it's"]"#);
        assert_eq!(py_str(Some(&json!(null))), "None");
        assert_eq!(py_int(&json!("5")), Ok(5));
        assert_eq!(py_int(&json!(true)), Ok(1));
        assert!(py_int(&json!("x")).is_err());
    }

    #[test]
    fn queries_are_parsed_as_parse_qs_parses_them() {
        let q = Query::parse("since=4&kinds=game%2Cpanel.console&params=%7B%22modes%22%3A%5B%22bus%22%5D%7D&track=&x=a+b&since=9");
        assert_eq!(q.get("since"), Some("4"));
        assert_eq!(q.get("kinds"), Some("game,panel.console"));
        assert_eq!(q.get("params"), Some(r#"{"modes":["bus"]}"#));
        assert_eq!(q.get("track"), None, "blank values are dropped");
        assert_eq!(q.int("track", 0), Ok(0));
        assert_eq!(q.get("x"), Some("a b"));
        assert!(Query::parse("limit=abc").int("limit", 1).is_err());
        assert_eq!(Query::parse("a=%zz%4").get("a"), Some("%zz%4"));
    }

    #[test]
    fn window_classes_come_from_the_header() {
        let header = "x\nenum class WindowClass : uint16_t {\n\tNone, ///< none\n\t/** main\n * - 0 = #MainWidgets\n */\n\tMainWindow = WindowClass::None,\n\
                      \tMainToolbar, // bar\n\tStatusbar,\n\tInvalid = 0xFFFF, ///< Invalid window.\n};\n";
        let v = window_classes(header).unwrap();
        assert_eq!(v["0"], json!({"name": "MainWindow", "aliases": ["None", "MainWindow"]}));
        assert_eq!(v["1"]["name"], "MainToolbar");
        assert_eq!(v["2"]["name"], "Statusbar");
        assert_eq!(v["65535"]["name"], "Invalid");
        let real = std::fs::read_to_string(here().join("../../../src/window_type.h")).unwrap();
        let v = window_classes(&real).unwrap();
        assert!(v.as_object().unwrap().len() > 100);
        assert_eq!((v["1"]["name"].as_str(), v["2"]["name"].as_str()), (Some("MainToolbar"), Some("Statusbar")));
    }

    #[test]
    fn rpc_status_maps_jsonrpc_errors() {
        assert_eq!(rpc_status(&json!({"game_tick": 3})), 200);
        assert_eq!(rpc_status(&json!({"error": {"jsonrpc": "2.0", "error": {"code": -32602, "message": "unknown"}}})), 404);
        assert_eq!(rpc_status(&json!({"error": {"jsonrpc": "2.0", "error": {"code": -32603}}})), 502);
        assert_eq!(rpc_status(&json!({"error": "refused by the tool"})), 200);
        assert_eq!(rpc_status(&json!({"ok": false, "error": {"code": 1}})), 200);
    }

    #[test]
    fn the_command_line_is_server_pys() {
        let a = parse_args(&["--bin", "/x/openttd", "--panel-port=18800", "--seed", "-3", "--auto-approve", "--no-plan-bot", "--ai", ""]
            .map(String::from)).unwrap();
        assert_eq!((a.bin.as_str(), a.panel_port, a.mcp_port, a.game_port, a.seed, a.ai.as_str(), a.auto_approve, a.plan_bot),
                   ("/x/openttd", 18800, 8731, 3979, -3, "", true, false));
        assert!(parse_args(&["--panel-port".into(), "1".into()]).is_err(), "--bin is required");
        assert!(parse_args(&["--bin".into(), "x".into(), "--nope".into()]).is_err());
    }

    #[test]
    fn a_record_is_mcp_instance_v1() {
        let r = registry_record("panel-1", &json!({"kind": "panel", "url": "http://127.0.0.1:1", "game_port": 3}));
        let schema: Value = serde_json::from_str(&std::fs::read_to_string(here().join("../schema/mcp-instance.v1.schema.json")).unwrap()).unwrap();
        for k in schema["required"].as_array().unwrap() { assert!(r.get(k.as_str().unwrap()).is_some(), "{k}"); }
        for k in r.as_object().unwrap().keys() { assert!(schema["properties"].get(k).is_some(), "{k} is not in the schema"); }
        assert_eq!(r["schema"], schema["properties"]["schema"]["const"]);
        assert!(r.get("token").is_none() && r.get("agent_token").is_none());
    }

    #[test]
    fn a_script_runs_in_process_and_streams_its_lines() {
        // No panel at this address: the derived functions are absent, the built-ins remain.
        let (tx, rx) = mpsc::channel();
        let h = spawn_script("http://127.0.0.1:9".into(), "print(\"a\\nb\"); 40 + 2".into(), Arc::new(AtomicBool::new(false)), tx);
        assert_eq!(h.join().unwrap(), 0);
        let lines: Vec<String> = rx.iter().map(|l| match l { ScriptLine::Out(s) => format!("out {s}"), ScriptLine::Err(s) => format!("err {s}"), ScriptLine::Exit(c) => format!("exit {c}") }).collect();
        assert_eq!(lines, ["out a", "out b", "out 42", "exit 0"]);
        let (tx, rx) = mpsc::channel();
        assert_eq!(spawn_script("http://127.0.0.1:9".into(), "bad(".into(), Arc::new(AtomicBool::new(false)), tx).join().unwrap(), 1);
        assert!(matches!(rx.recv().unwrap(), ScriptLine::Err(s) if s.starts_with("mcp-rhai: Syntax error")));
        let (tx, _rx) = mpsc::channel();
        let abort = Arc::new(AtomicBool::new(true));
        assert_eq!(spawn_script("http://127.0.0.1:9".into(), "loop { }".into(), abort, tx).join().unwrap(), 1, "abort stops a script");
    }

    /// A GUI client's MCP server: tools/list, and tools/call answered per tool, as the game answers.
    fn fake_client() -> String {
        let l = TcpListener::bind("127.0.0.1:0").unwrap();
        let endpoint = format!("http://{}/mcp", l.local_addr().unwrap());
        thread::spawn(move || for mut c in l.incoming().flatten() {
            let Some(req) = read_request(&mut c) else { continue };
            let auth = req.header("Authorization").unwrap_or("").to_string();
            let call: Value = serde_json::from_slice(&req.body).unwrap_or(Value::Null);
            let text = |v: Value| json!({"jsonrpc": "2.0", "id": 1, "result": {"content": [{"type": "text", "text": v.to_string()}]}});
            let out = if auth != "Bearer t0k" {
                json!({"jsonrpc": "2.0", "id": 1, "error": {"code": -32001, "message": "bad token"}})
            } else if call["method"] == "tools/list" {
                json!({"jsonrpc": "2.0", "id": 1, "result": {"tools": [{"name": "openttd.view.state", "inputSchema": {"type": "object"}}]}})
            } else {
                match call["params"]["name"].as_str().unwrap_or("") {
                    "openttd.view.state" => text(json!({"ok": true, "x": 64, "args": call["params"]["arguments"]})),
                    "openttd.view.ask.answers" => text(json!({"asks": [{"id": 2, "question": "r", "answer": null},
                                                                       {"id": 1, "question": "q", "answer": "yes"}]})),
                    other => text(json!({"ok": false, "error": format!("unknown tool {other}")})),
                }
            };
            hub::respond(c, 200, "application/json", out.to_string().as_bytes());
        });
        endpoint
    }

    #[test]
    fn the_owners_window_is_reached_through_its_token_file() {
        let dir = std::env::temp_dir().join(format!("mcp-panel-test-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let token = dir.join("view-token.json");
        std::fs::write(&token, json!({"agent_token": "t0k", "endpoint": fake_client()}).to_string()).unwrap();
        let args = parse_args(&["--bin", "/nonexistent/openttd", "--panel-port", "1"].map(String::from)).unwrap();
        let g = Game::new(args);
        let p = Panel { game: g.clone(), routes: routes() };
        let post = |path: &str, body: Value| p.dispatch(&Request { method: "POST".into(), path: path.into(), query: Query::default(),
                                                                    headers: vec![], body: vec![] }, body);
        let get = |path: &str| p.dispatch(&Request { method: "GET".into(), path: path.into(), query: Query::default(), headers: vec![], body: vec![] }, Value::Null);
        // Without a client, a route that needs one says so with 503.
        assert_eq!(post("/api/view/tool", json!({"name": "openttd.view.state"})).0, 503);
        assert!(g.client_tools().is_empty());
        *g.view_token.lock().unwrap() = Some(token.display().to_string());
        let (code, out) = post("/api/view/tool", json!({"name": "openttd.view.state", "arguments": {"a": 1}}));
        assert_eq!((code, out["ok"].clone(), out["args"].clone()), (200, json!(true), json!({"a": 1})));
        assert_eq!(post("/api/view/tool", json!({"name": "openttd.view.nope"})).1["ok"], json!(false));
        assert_eq!(post("/api/view/tool", json!({})).0, 400, "the body is checked first");
        // The client's tools are derived routes once the game's MCP is up.
        assert_eq!(post("/api/client/openttd.view.state", json!({})).0, 503, "MCP not up yet");
        g.mcp_up.store(true, Ordering::SeqCst);
        assert_eq!(post("/api/client/openttd.view.state", json!({})).1["x"], 64);
        assert_eq!(g.client_tools().len(), 1);
        assert_eq!(derive_routes(&[], &g.client_tools(), &[])[0].path, "/api/client/openttd.view.state");
        // An answered question is published once, an open one not at all.
        let since = g.bus.seq();
        assert_eq!(get("/api/ask").0, 200);
        assert_eq!(get("/api/ask").0, 200);
        let answered = g.bus.since(since, &kinds_of("ask"), 10);
        assert_eq!(answered.len(), 1);
        assert_eq!(answered[0]["data"]["question"], "q");
        // The game's own tools need its token: without one the call fails as a 502, never a hang.
        assert_eq!(post("/api/tools/openttd.game.set_pause", json!({"pause": true})).0, 502);
        assert_eq!(get("/api/resources/game/state").0, 502);
        assert_eq!(get("/api/nothing").0, 404);
        let _ = std::fs::remove_dir_all(&dir);
    }
}
