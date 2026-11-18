//! RESEARCH-ONLY: one long-lived AlgoGlyph `flow serve` for every kernel call of a process (I0086).
//!
//! `npm run flow -- kernel <id> -` boots Vite and AlgoGlyph's registry for every call; a planned bot
//! start (mcp-plan --track 16) makes one track-planning call per line and then knapsack-dp, each its
//! own boot. `flow serve` (AlgoGlyph scripts/flow.mjs) answers one JSON request per stdin line with
//! one JSON line, for as long as stdin stays open; mcp-human's `Flow` (src/algoglyph.rs there)
//! already drives it. This is the same client for mcp-rhai, generic over the kernel: it knows the
//! serve protocol and nothing of OpenTTD.
//!
//! Switch: `MCP_ALGOGLYPH_SERVE=1` (or `on`, `true`). Off by default, so every caller keeps the
//! per-call runner until the switch is chosen. A serve that cannot start, ends, or answers garbage
//! is dropped for the rest of the process and the caller falls back to the per-call runner; the
//! fallback is counted and reported by `summary()`, never silent.
use serde_json::{json, Value};
use std::io::{BufRead, BufReader, Write};
use std::path::Path;
use std::process::{Child, ChildStdin, ChildStdout, Command, Stdio};
use std::sync::{Mutex, OnceLock};

/// Whether the switch asks for one long-lived serve.
pub fn wanted() -> bool {
    std::env::var("MCP_ALGOGLYPH_SERVE").is_ok_and(|v| matches!(v.trim(), "1" | "on" | "true" | "yes"))
}

/// A running `flow serve`.
pub struct Serve {
    child: Child,
    stdin: ChildStdin,
    stdout: BufReader<ChildStdout>,
    next: u64,
}

impl Serve {
    /// Start `npm run --silent flow -- serve` in the AlgoGlyph checkout `dir`.
    pub fn start(dir: &Path) -> Result<Serve, String> {
        if !dir.join("package.json").is_file() {
            return Err(format!("no AlgoGlyph checkout at {}", dir.display()));
        }
        Self::spawn(Command::new("npm").args(["run", "--silent", "flow", "--", "serve"]).current_dir(dir))
    }

    /// Any process that speaks the serve protocol (a test stands one in).
    pub fn spawn(cmd: &mut Command) -> Result<Serve, String> {
        let mut child = cmd.stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::null()).spawn()
            .map_err(|e| format!("flow serve: {e}"))?;
        let stdin = child.stdin.take().ok_or("flow serve: no stdin")?;
        let stdout = BufReader::new(child.stdout.take().ok_or("flow serve: no stdout")?);
        Ok(Serve { child, stdin, stdout, next: 1 })
    }

    pub fn pid(&self) -> u32 { self.child.id() }

    /// One request {id, cmd, ...}: a line in, the line with the same id out.
    pub fn request(&mut self, mut q: Value) -> Result<Value, String> {
        let id = self.next;
        self.next += 1;
        q["id"] = json!(id);
        writeln!(self.stdin, "{q}").and_then(|_| self.stdin.flush()).map_err(|e| format!("flow serve: {e}"))?;
        loop {
            let mut line = String::new();
            if self.stdout.read_line(&mut line).map_err(|e| format!("flow serve: {e}"))? == 0 {
                return Err("flow serve: the server ended".into());
            }
            if line.trim().is_empty() { continue; }
            let mut v: Value = serde_json::from_str(&line).map_err(|e| format!("flow serve: not JSON ({e})"))?;
            if v.get("id") != Some(&json!(id)) {
                return Err(format!("flow serve: answer for id {} while waiting for {id}", v.get("id").unwrap_or(&Value::Null)));
            }
            if let Some(m) = v.as_object_mut() { m.remove("id"); }
            return Ok(v);
        }
    }

    /// One kernel on an input; the answer exactly as `flow -- kernel <id> -` prints it.
    pub fn kernel(&mut self, kernel: &str, input: &Value) -> Result<Value, String> {
        self.request(json!({"cmd": "kernel", "kernel": kernel, "input": input}))
    }
}

impl Drop for Serve {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

/// The process's one serve, and what it did.
struct Shared {
    serve: Option<Serve>,
    /// Why the serve is gone (it is then not restarted), or None while it runs or before it started.
    gone: Option<String>,
    pid: Option<u32>,
    calls: u64,
    fallbacks: u64,
}

fn shared() -> &'static Mutex<Shared> {
    static S: OnceLock<Mutex<Shared>> = OnceLock::new();
    S.get_or_init(|| Mutex::new(Shared { serve: None, gone: None, pid: None, calls: 0, fallbacks: 0 }))
}

/// One kernel call through the process's serve, started on first use in `dir`. Err(reason) when the
/// serve is unavailable; the caller then runs the kernel its own way and says so (`fell_back`).
pub fn kernel(dir: &Path, kernel: &str, input: &Value) -> Result<Value, String> {
    let mut s = shared().lock().unwrap_or_else(|p| p.into_inner());
    if let Some(why) = &s.gone { return Err(why.clone()); }
    if s.serve.is_none() {
        match Serve::start(dir) {
            Ok(v) => { s.pid = Some(v.pid()); s.serve = Some(v); }
            Err(e) => { s.gone = Some(e.clone()); return Err(e); }
        }
    }
    let r = s.serve.as_mut().map(|v| v.kernel(kernel, input)).unwrap_or_else(|| Err("flow serve: none".into()));
    match r {
        Ok(v) => { s.calls += 1; Ok(v) }
        Err(e) => { s.serve = None; s.gone = Some(e.clone()); Err(e) }
    }
}

/// Count a call the caller ran with the per-call runner after `kernel` refused.
#[allow(dead_code)] // not every binary that includes this module has a per-call runner
pub fn fell_back() {
    shared().lock().unwrap_or_else(|p| p.into_inner()).fallbacks += 1;
}

/// What answered this process's kernel calls: {runner, pid, calls, fallbacks, gone}.
#[allow(dead_code)] // mcp-intent includes cargo.rs and never reports its runner
pub fn summary() -> Value {
    let s = shared().lock().unwrap_or_else(|p| p.into_inner());
    let runner = match (s.pid, wanted()) {
        (Some(_), _) => "flow serve, one process for every call",
        (None, true) => "flow serve asked for (MCP_ALGOGLYPH_SERVE), not started",
        (None, false) => "npm run flow -- kernel, one process per call",
    };
    json!({"runner": runner,
           "pid": s.pid, "calls": s.calls, "fallbacks": s.fallbacks, "gone": s.gone})
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A stand-in for `flow serve`: echoes each request's id with the kernel name, line by line.
    fn fake() -> Serve {
        let script = r#"while IFS= read -r l; do id=$(printf '%s' "$l" | sed -E 's/.*"id":([0-9]+).*/\1/'); k=$(printf '%s' "$l" | sed -E 's/.*"kernel":"([^"]*)".*/\1/'); printf '{"id":%s,"ok":true,"kernel":{"id":"%s"}}\n' "$id" "$k"; done"#;
        Serve::spawn(Command::new("sh").args(["-c", script])).expect("sh")
    }

    #[test]
    fn one_process_answers_many_calls_each_by_its_own_id() {
        let mut s = fake();
        let pid = s.pid();
        for k in ["knapsack-dp", "track-planning", "transport-investment"] {
            let v = s.kernel(k, &json!({"weights": [1]})).unwrap();
            assert_eq!(v["kernel"]["id"], k);
            assert!(v.get("id").is_none(), "the serve's id is not part of the answer");
        }
        assert_eq!(s.pid(), pid);
    }

    #[test]
    fn a_serve_that_ended_is_an_error_not_an_answer() {
        let mut s = Serve::spawn(Command::new("sh").args(["-c", "exit 0"])).unwrap();
        let _ = s.child.wait();
        assert!(s.kernel("knapsack-dp", &json!({})).is_err());
    }

    #[test]
    fn the_switch_is_off_unless_asked() {
        // Read only: the variable is not set by the test runner.
        if std::env::var_os("MCP_ALGOGLYPH_SERVE").is_none() { assert!(!wanted()); }
    }
}
