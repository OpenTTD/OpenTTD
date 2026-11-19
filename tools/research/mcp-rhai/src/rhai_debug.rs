//! RESEARCH-ONLY: a debugger for Rhai scripts in the panel (I0090, research/rhai-ide.md).
//!
//! A debug session runs one script in its own thread, in the engine every panel script runs in,
//! with rhai's debugging interface attached. On every step the callback decides whether to pause:
//! at a breakpoint line of the script (not of a library it imports), or as the last command asked
//! (step into: the next line; step over: the next line at the same or a shallower call depth; step
//! out: the first line after the current function returns). While paused, the script's thread
//! waits for the next command, and the session answers where it stands: the line, the variables in
//! scope and the call stack. Nothing here pauses the game: a script changes the game only through
//! approval-gated tools, as always.

use std::collections::{BTreeMap, BTreeSet};
use std::sync::{Arc, Condvar, Mutex, OnceLock};
use std::time::{Duration, Instant};

use rhai::debugger::{DebuggerCommand, DebuggerEvent};
use rhai::Engine;
use serde_json::{json, Value};

/// How long a paused script waits for a command before the session stops it.
pub const IDLE_STOP: Duration = Duration::from_secs(30 * 60);
/// The most sessions that run at once; a finished one is forgotten after ten minutes.
pub const MAX_SESSIONS: usize = 8;
const FORGET: Duration = Duration::from_secs(600);
/// A variable's value as shown: its text, cut at this many characters.
const SHOW: usize = 400;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Mode {
    Run,
    Into,
    Over(usize),
    Out(usize),
    Stop,
}

#[derive(Clone, Debug)]
pub struct Pause {
    pub line: usize,
    pub column: usize,
    pub source: Option<String>,
    pub depth: usize,
    pub vars: Vec<Value>,
    pub stack: Vec<Value>,
    pub node: String,
}

struct Inner {
    breakpoints: BTreeSet<usize>,
    mode: Mode,
    paused: Option<Pause>,
    output: Vec<String>,
    finished: Option<Value>,
    last: (usize, usize, Option<String>),
    pauses: u64,
}

pub struct Session {
    pub id: u64,
    inner: Mutex<Inner>,
    cv: Condvar,
    created: Instant,
}

/// What a value shows as: its type and its text, cut.
fn show(v: &rhai::Dynamic) -> Value {
    let text = v.to_string();
    let text = if text.chars().count() > SHOW { format!("{}…", text.chars().take(SHOW).collect::<String>()) } else { text };
    json!({"type": v.type_name(), "value": text})
}

impl Session {
    pub fn new(id: u64, breakpoints: impl IntoIterator<Item = usize>) -> Arc<Session> {
        Arc::new(Session {
            id,
            inner: Mutex::new(Inner {
                breakpoints: breakpoints.into_iter().collect(), mode: Mode::Run, paused: None,
                output: Vec::new(), finished: None, last: (0, 0, None), pauses: 0,
            }),
            cv: Condvar::new(),
            created: Instant::now(),
        })
    }

    /// Attach this session to `engine`: its printed lines and its debugger callback.
    pub fn attach(self: &Arc<Self>, engine: &mut Engine) {
        let out = self.clone();
        engine.on_print(move |s| { if let Ok(mut g) = out.inner.lock() { g.output.extend(s.split('\n').map(str::to_string)); } });
        let me = self.clone();
        // rhai marks this API volatile (a deprecation warning that says it is not deprecated):
        // the debugger is research tooling and moves with the rhai version Cargo.lock pins.
        #[allow(deprecated)]
        engine.register_debugger(|_, d| d, move |ctx, event, node, source, pos| {
            if matches!(event, DebuggerEvent::Start | DebuggerEvent::End) { return Ok(DebuggerCommand::StepInto); }
            // A block's opening brace and the no-op a function is entered through are not lines a
            // person stops on: the first statement inside is.
            if matches!(node, rhai::ASTNode::Stmt(rhai::Stmt::Block(..) | rhai::Stmt::Noop(..))) { return Ok(DebuggerCommand::StepInto); }
            let Some(line) = pos.line() else { return Ok(DebuggerCommand::StepInto) };
            let stack = ctx.global_runtime_state().debugger().call_stack();
            let depth = stack.len();
            let src = source.map(str::to_string);
            let mut g = me.inner.lock().map_err(|_| "the debug session's lock is poisoned".to_string())?;
            if g.mode == Mode::Stop { return Err("stopped by the debugger".into()); }
            let here = (line, depth, src.clone());
            if g.last == here { return Ok(DebuggerCommand::StepInto); }
            // A breakpoint fires once per arrival on its line, whatever calls the line makes.
            let new_line = g.last.0 != line || g.last.2 != src;
            g.last = here;
            let at_break = src.is_none() && new_line && g.breakpoints.contains(&line);
            let pause = match g.mode {
                Mode::Run => at_break,
                Mode::Into => true,
                Mode::Over(d) => depth <= d || at_break,
                Mode::Out(d) => depth < d || at_break,
                Mode::Stop => false,
            };
            if !pause { return Ok(DebuggerCommand::StepInto); }
            let vars = ctx.scope().iter().map(|(name, constant, v)| {
                let mut s = show(&v);
                s["name"] = json!(name);
                s["constant"] = json!(constant);
                s
            }).collect();
            let frames = stack.iter().rev().map(|f| json!({
                "function": f.fn_name.as_str(), "source": f.source.as_ref().map(|s| s.as_str()),
                "line": f.pos.line(), "args": f.args.iter().map(show).collect::<Vec<_>>(),
            })).collect();
            let kind = format!("{node:?}");
            let node = kind.split(['(', ' ', '{']).next().unwrap_or("").to_string() + "::" + kind.split(['(', '{']).nth(1).unwrap_or("").split([' ', '(', ',']).next().unwrap_or("");
            g.paused = Some(Pause { line, column: pos.position().unwrap_or(0), source: src, depth, vars, stack: frames, node });
            g.pauses += 1;
            me.cv.notify_all();
            let deadline = Instant::now() + IDLE_STOP;
            while g.paused.is_some() && g.mode != Mode::Stop {
                let left = deadline.saturating_duration_since(Instant::now());
                if left.is_zero() { g.mode = Mode::Stop; break; }
                g = me.cv.wait_timeout(g, left.min(Duration::from_secs(1))).map_err(|_| "the debug session's lock is poisoned".to_string())?.0;
            }
            if g.mode == Mode::Stop { g.paused = None; return Err("stopped by the debugger".into()); }
            Ok(DebuggerCommand::StepInto)
        });
    }

    /// Run `script` in `engine` (with this session attached) to its end, on this thread.
    pub fn run(self: &Arc<Self>, mut engine: Engine, script: &str) {
        self.attach(&mut engine);
        let result = engine.eval::<rhai::Dynamic>(script);
        let mut g = self.inner.lock().unwrap_or_else(|e| e.into_inner());
        g.paused = None;
        g.finished = Some(match result {
            Ok(v) => json!({"ok": true, "value": show(&v)}),
            Err(e) => json!({"ok": false, "error": e.to_string(), "stopped": g.mode == Mode::Stop}),
        });
        self.cv.notify_all();
    }

    /// Apply a command: continue, step (into), over, out, or stop; and new breakpoints if given.
    /// Refused while the script runs (only `stop` and breakpoints are accepted then).
    pub fn command(&self, cmd: &str, breakpoints: Option<Vec<usize>>) -> Result<(), String> {
        let mut g = self.inner.lock().map_err(|_| "the debug session's lock is poisoned".to_string())?;
        if let Some(b) = breakpoints { g.breakpoints = b.into_iter().collect(); }
        if g.finished.is_some() { return if cmd.is_empty() { Ok(()) } else { Err("the script has finished".into()) }; }
        let depth = g.paused.as_ref().map(|p| p.depth);
        g.mode = match (cmd, depth) {
            ("", _) => return Ok(()),
            ("stop", _) => Mode::Stop,
            (_, None) => return Err(format!("{cmd}: the script is running, not paused (only stop works now)")),
            ("continue", _) => Mode::Run,
            ("step", _) => Mode::Into,
            ("over", Some(d)) => Mode::Over(d),
            ("out", Some(d)) => Mode::Out(d),
            (other, _) => return Err(format!("{other}: not a command (continue, step, over, out, stop)")),
        };
        g.paused = None;
        self.cv.notify_all();
        Ok(())
    }

    /// Wait up to `timeout` until the script is paused or has finished.
    pub fn settle(&self, timeout: Duration) {
        let deadline = Instant::now() + timeout;
        let mut g = self.inner.lock().unwrap_or_else(|e| e.into_inner());
        while g.paused.is_none() && g.finished.is_none() {
            let left = deadline.saturating_duration_since(Instant::now());
            if left.is_zero() { break; }
            g = self.cv.wait_timeout(g, left).unwrap_or_else(|e| e.into_inner()).0;
        }
    }

    /// Where the session stands, as the page and the API read it.
    pub fn snapshot(&self) -> Value {
        let g = self.inner.lock().unwrap_or_else(|e| e.into_inner());
        let status = if g.finished.is_some() { "finished" } else if g.paused.is_some() { "paused" } else { "running" };
        let mut v = json!({"id": self.id, "status": status, "breakpoints": g.breakpoints, "output": g.output,
                           "pauses": g.pauses, "finished": g.finished});
        if let Some(p) = &g.paused {
            v["line"] = json!(p.line);
            v["column"] = json!(p.column);
            v["source"] = json!(p.source);
            v["depth"] = json!(p.depth);
            v["vars"] = json!(p.vars);
            v["stack"] = json!(p.stack);
            v["node"] = json!(p.node);
        }
        v
    }

    fn done_long_ago(&self) -> bool {
        self.inner.lock().map(|g| g.finished.is_some()).unwrap_or(true) && self.created.elapsed() > FORGET
    }

    fn is_live(&self) -> bool {
        self.inner.lock().map(|g| g.finished.is_none()).unwrap_or(false)
    }
}

fn sessions() -> &'static Mutex<BTreeMap<u64, Arc<Session>>> {
    static S: OnceLock<Mutex<BTreeMap<u64, Arc<Session>>>> = OnceLock::new();
    S.get_or_init(|| Mutex::new(BTreeMap::new()))
}

fn session(id: u64) -> Option<Arc<Session>> {
    sessions().lock().ok()?.get(&id).cloned()
}

fn lines_of(v: Option<&Value>) -> Result<Vec<usize>, String> {
    match v {
        None | Some(Value::Null) => Ok(Vec::new()),
        Some(Value::Array(a)) => a.iter().map(|x| x.as_u64().filter(|n| *n > 0).map(|n| n as usize).ok_or_else(|| "breakpoints: line numbers from 1".to_string())).collect(),
        Some(_) => Err("breakpoints: an array of line numbers".into()),
    }
}

/// How long a request waits for the script to pause or finish before it answers "running".
const SETTLE: Duration = Duration::from_secs(3);

/// POST /api/rhai/debug {script, breakpoints?}: start a session; answers it once it pauses or
/// finishes (or after 3 s, running).
pub fn h_debug_start(g: &Arc<crate::panel::Game>, i: &crate::panel::Input) -> Result<(u16, Value), String> {
    let script = i.body.get("script").and_then(Value::as_str).unwrap_or("").to_string();
    let bps = match lines_of(i.body.get("breakpoints")) { Ok(b) => b, Err(e) => return Ok((400, json!({"error": e}))) };
    let s = {
        let mut all = sessions().lock().map_err(|_| "the sessions' lock is poisoned".to_string())?;
        all.retain(|_, s| !s.done_long_ago());
        if all.values().filter(|s| s.is_live()).count() >= MAX_SESSIONS {
            return Ok((429, json!({"error": format!("at most {MAX_SESSIONS} debug sessions run at once: stop one first")})));
        }
        let id = all.keys().next_back().map_or(1, |k| k + 1);
        let s = Session::new(id, bps);
        all.insert(id, s.clone());
        s
    };
    let base = g.panel_url();
    let hub = std::env::var("MCP_HUB").ok().filter(|h| !h.is_empty());
    let run = s.clone();
    std::thread::spawn(move || {
        let engine = crate::rhai_host::host(&base, hub.as_deref());
        run.run(engine, &script);
    });
    g.event("rhai", &format!("debug session {} started", s.id));
    s.settle(SETTLE);
    Ok((200, s.snapshot()))
}

/// GET /api/rhai/debug?id=: a session's state (every live session without an id).
pub fn h_debug(_: &Arc<crate::panel::Game>, i: &crate::panel::Input) -> Result<(u16, Value), String> {
    match i.query.get("id") {
        Some(id) => {
            let id: u64 = id.parse().map_err(|_| format!("id {id:?}: a session number"))?;
            Ok(match session(id) { Some(s) => (200, s.snapshot()), None => (404, json!({"error": format!("no debug session {id}")})) })
        }
        None => {
            let all = sessions().lock().map_err(|_| "the sessions' lock is poisoned".to_string())?;
            Ok((200, json!({"sessions": all.values().map(|s| s.snapshot()).collect::<Vec<_>>()})))
        }
    }
}

/// POST /api/rhai/debug/command {id, command, breakpoints?}: continue, step, over, out or stop;
/// answers the session once it pauses again or finishes (or after 3 s, running).
pub fn h_debug_command(_: &Arc<crate::panel::Game>, i: &crate::panel::Input) -> Result<(u16, Value), String> {
    let id = i.body.get("id").and_then(Value::as_u64).unwrap_or(0);
    let cmd = i.body.get("command").and_then(Value::as_str).unwrap_or("");
    let bps = match i.body.get("breakpoints") { None => None, v => match lines_of(v) { Ok(b) => Some(b), Err(e) => return Ok((400, json!({"error": e}))) } };
    let Some(s) = session(id) else { return Ok((404, json!({"error": format!("no debug session {id}")}))) };
    if let Err(e) = s.command(cmd, bps) { return Ok((409, json!({"error": e, "session": s.snapshot()}))); }
    if !cmd.is_empty() {
        std::thread::sleep(Duration::from_millis(20));
        s.settle(SETTLE);
    }
    Ok((200, s.snapshot()))
}

#[cfg(test)]
mod tests {
    use super::*;

    const SCRIPT: &str = "fn add(a, b) {\n    let s = a + b;\n    s\n}\nlet x = 1;\nlet y = add(x, 2);\nlet z = y * 10;\nprint(z);\nz\n";

    fn start(bps: &[usize]) -> Arc<Session> {
        let s = Session::new(1, bps.iter().copied());
        let run = s.clone();
        std::thread::spawn(move || run.run(Engine::new(), SCRIPT));
        s.settle(Duration::from_secs(5));
        s
    }

    fn var(v: &Value, name: &str) -> Option<String> {
        v["vars"].as_array()?.iter().rev().find(|x| x["name"] == name).and_then(|x| x["value"].as_str().map(str::to_string))
    }

    fn step(s: &Session, cmd: &str) -> Value {
        s.command(cmd, None).unwrap();
        std::thread::sleep(Duration::from_millis(10));
        s.settle(Duration::from_secs(5));
        s.snapshot()
    }

    #[test]
    fn a_breakpoint_pauses_with_the_scope() {
        let s = start(&[6]);
        let v = s.snapshot();
        assert_eq!((v["status"].as_str(), v["line"].as_u64(), v["depth"].as_u64()), (Some("paused"), Some(6), Some(0)), "{v}");
        assert_eq!(var(&v, "x").as_deref(), Some("1"));
        assert_eq!(var(&v, "y"), None, "y is not declared yet");
        let end = step(&s, "continue");
        assert_eq!(end["status"], "finished");
        assert_eq!(end["finished"]["value"]["value"], "30");
        assert_eq!(end["output"], json!(["30"]));
    }

    #[test]
    fn step_into_enters_the_function_and_out_returns() {
        let s = start(&[6]);
        let inside = step(&s, "step");
        assert_eq!((inside["line"].as_u64(), inside["depth"].as_u64()), (Some(2), Some(1)), "{inside}");
        assert_eq!(inside["stack"][0]["function"], "add");
        assert_eq!(var(&inside, "a").as_deref(), Some("1"));
        assert_eq!(var(&inside, "b").as_deref(), Some("2"));
        let back = step(&s, "out");
        assert_eq!(back["depth"].as_u64(), Some(0), "{back}");
        assert!(back["line"].as_u64().unwrap() >= 6);
        step(&s, "stop");
    }

    #[test]
    fn step_over_stays_at_the_same_depth() {
        let s = start(&[6]);
        let next = step(&s, "over");
        assert_eq!((next["line"].as_u64(), next["depth"].as_u64()), (Some(7), Some(0)), "{next}");
        assert_eq!(var(&next, "y").as_deref(), Some("3"));
        let after = step(&s, "over");
        assert_eq!(after["line"].as_u64(), Some(8));
        assert_eq!(var(&after, "z").as_deref(), Some("30"));
        assert_eq!(step(&s, "continue")["status"], "finished");
    }

    #[test]
    fn stop_ends_a_paused_script_with_an_error() {
        let s = start(&[5]);
        assert_eq!(s.snapshot()["line"].as_u64(), Some(5));
        let end = step(&s, "stop");
        assert_eq!(end["status"], "finished");
        assert_eq!(end["finished"]["ok"], false);
        assert_eq!(end["finished"]["stopped"], true);
        assert!(s.command("step", None).is_err(), "a finished script takes no command");
    }

    #[test]
    fn breakpoints_change_while_paused_and_commands_are_checked() {
        let s = start(&[5]);
        assert!(s.command("jump", None).is_err());
        s.command("", Some(vec![8])).unwrap();
        let at = step(&s, "continue");
        assert_eq!(at["line"].as_u64(), Some(8), "the new breakpoint, not the old one: {at}");
        assert_eq!(at["breakpoints"], json!([8]));
        assert_eq!(step(&s, "continue")["status"], "finished");
    }

    #[test]
    fn without_breakpoints_a_script_runs_through() {
        let s = start(&[]);
        let v = s.snapshot();
        assert_eq!(v["status"], "finished", "{v}");
        assert_eq!(v["pauses"], 0);
    }

    #[test]
    fn breakpoint_lists_are_line_numbers() {
        assert_eq!(lines_of(Some(&json!([3, 7]))), Ok(vec![3, 7]));
        assert_eq!(lines_of(None), Ok(vec![]));
        assert!(lines_of(Some(&json!([0]))).is_err());
        assert!(lines_of(Some(&json!("3"))).is_err());
        assert!(lines_of(Some(&json!([-1]))).is_err());
    }
}
