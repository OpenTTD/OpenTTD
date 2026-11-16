//! The motion model as an AlgoGlyph kernel: when MCP_ALGOGLYPH_DIR names an AlgoGlyph checkout
//! with the flow CLI, every pointer movement comes from its `human-pointing` kernel (one JSON line
//! per move to `npm run --silent flow -- serve`), and from motion.rs otherwise. Both are held to the
//! same golden vectors (golden/human-pointing.v1.json), so the cursor moves bit for bit the same
//! either way; the kernel is used only with the default model constants, which it shares.
//! Any failure of the kernel (missing checkout, a crash, a malformed answer) falls back to the
//! builtin model for the rest of the run, and says so once.

use crate::motion::Sample;
use serde_json::{json, Value};
use std::io::{BufRead, BufReader, Write};
use std::process::{Child, ChildStdin, ChildStdout, Command, Stdio};

/// A running `flow serve` process.
pub struct Flow {
    child: Child,
    stdin: ChildStdin,
    stdout: BufReader<ChildStdout>,
}

impl Flow {
    /// Start the flow server of the checkout MCP_ALGOGLYPH_DIR names; None when unset or absent.
    pub fn from_env() -> Option<Flow> {
        let dir = std::env::var("MCP_ALGOGLYPH_DIR").ok().filter(|d| std::path::Path::new(d).join("package.json").exists())?;
        let mut child = Command::new("npm").args(["run", "--silent", "flow", "--", "serve"]).current_dir(&dir)
            .stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::null()).spawn().ok()?;
        let stdin = child.stdin.take()?;
        let stdout = BufReader::new(child.stdout.take()?);
        Some(Flow { child, stdin, stdout })
    }

    /// One kernel call: a line in, a line out.
    pub fn kernel(&mut self, kernel: &str, input: Value) -> Result<Value, String> {
        let line = json!({"cmd": "kernel", "kernel": kernel, "input": input}).to_string();
        writeln!(self.stdin, "{line}").and_then(|_| self.stdin.flush()).map_err(|e| format!("flow: {e}"))?;
        let mut out = String::new();
        if self.stdout.read_line(&mut out).map_err(|e| format!("flow: {e}"))? == 0 {
            return Err("flow: the server ended".into());
        }
        serde_json::from_str(&out).map_err(|e| format!("flow: {e}: {out}"))
    }

    /// A movement from the human-pointing kernel, with the same inputs as motion.rs.
    pub fn pointing(&mut self, from: (i32, i32), to: (i32, i32), width: f64, seed: u64, drag: bool) -> Result<Vec<Sample>, String> {
        // A drag has no target width (the builtin ignores it); the kernel wants a positive one.
        let width = if drag { width.max(1.0) } else { width };
        let v = self.kernel("human-pointing", json!({"from": [from.0, from.1], "to": [to.0, to.1], "width": width, "seed": seed, "drag": drag}))?;
        samples(&v)
    }
}

impl Drop for Flow {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

/// The samples of a kernel answer: {ok, result: {ok, samples: [[x, y, wait_ms], ...]}}.
pub fn samples(v: &Value) -> Result<Vec<Sample>, String> {
    let r = v.get("result").unwrap_or(v);
    if v.get("ok") == Some(&Value::Bool(false)) || r.get("ok") == Some(&Value::Bool(false)) {
        return Err(format!("human-pointing: {}", v));
    }
    r.get("samples").and_then(Value::as_array).ok_or_else(|| format!("human-pointing: no samples in {v}"))?.iter()
        .map(|s| match s.as_array().map(Vec::as_slice) {
            Some([x, y, w]) => Ok(Sample { x: x.as_i64().ok_or("x")? as i32, y: y.as_i64().ok_or("y")? as i32, wait_ms: w.as_u64().ok_or("wait_ms")? }),
            _ => Err(format!("human-pointing: a malformed sample {s}")),
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reads_the_kernels_answer_shape() {
        let v = json!({"ok": true, "result": {"ok": true, "samples": [[1, 2, 8], [3, 4, 0]], "duration_ms": 8}});
        assert_eq!(samples(&v).unwrap(), vec![Sample { x: 1, y: 2, wait_ms: 8 }, Sample { x: 3, y: 4, wait_ms: 0 }]);
        assert!(samples(&json!({"ok": false, "error": "no"})).is_err());
    }

    /// Parity with the golden file through the live kernel; runs only where MCP_ALGOGLYPH_DIR is set.
    #[test]
    fn the_kernel_matches_the_golden_vectors() {
        let Some(mut flow) = Flow::from_env() else {
            eprintln!("skip: MCP_ALGOGLYPH_DIR is not an AlgoGlyph checkout");
            return;
        };
        let golden: Value = serde_json::from_str(include_str!("../golden/human-pointing.v1.json")).unwrap();
        for c in golden["cases"].as_array().unwrap() {
            let p = |k: &str| (c[k][0].as_i64().unwrap() as i32, c[k][1].as_i64().unwrap() as i32);
            let got = flow.pointing(p("from"), p("to"), c["width"].as_f64().unwrap(), c["seed"].as_u64().unwrap(), c["drag"].as_bool().unwrap()).unwrap();
            let want = samples(&json!({"samples": c["samples"]})).unwrap();
            assert_eq!(got, want, "case {c}");
        }
    }
}
