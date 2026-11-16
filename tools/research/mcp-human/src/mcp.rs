//! One OpenTTD process's own MCP server, addressed by the token file that
//! `prismatic_mcp start <port> <file>` writes ({"agent_token", "endpoint"}). The file is reread
//! on every call, so a client that restarts (and writes a fresh token) is picked up at once.

use serde_json::{json, Value};
use std::time::Duration;

#[derive(Clone)]
pub struct Mcp {
    token_file: String,
}

impl Mcp {
    pub fn new(token_file: &str) -> Self {
        Mcp { token_file: token_file.to_string() }
    }

    pub fn token_file(&self) -> &str {
        &self.token_file
    }

    fn credentials(&self) -> Result<(String, String), String> {
        let text = std::fs::read_to_string(&self.token_file).map_err(|e| format!("{}: {e}", self.token_file))?;
        let v: Value = serde_json::from_str(&text).map_err(|e| format!("{}: {e}", self.token_file))?;
        let endpoint = v.get("endpoint").and_then(Value::as_str).ok_or("token file has no endpoint")?;
        let token = v.get("agent_token").and_then(Value::as_str).ok_or("token file has no agent_token")?;
        Ok((endpoint.to_string(), token.to_string()))
    }

    fn rpc(&self, method: &str, params: Value) -> Result<Value, String> {
        let (endpoint, token) = self.credentials()?;
        ureq::post(&endpoint)
            .timeout(Duration::from_secs(15))
            .set("Authorization", &format!("Bearer {token}"))
            .set("Accept", "application/json, text/event-stream")
            .send_json(json!({"jsonrpc": "2.0", "id": 1, "method": method, "params": params}))
            .map_err(|e| format!("{endpoint}: {e}"))?
            .into_json::<Value>()
            .map_err(|e| format!("{endpoint}: {e}"))
    }

    /// Call a tool and parse its JSON answer. A tool that answers `ok: false` is an error.
    pub fn call(&self, tool: &str, args: Value) -> Result<Value, String> {
        let v = self.rpc("tools/call", json!({"name": tool, "arguments": args}))?;
        if let Some(err) = v.get("error") {
            return Err(format!("{tool}: {err}"));
        }
        let text = v.pointer("/result/content/0/text").and_then(Value::as_str).ok_or_else(|| format!("{tool}: no content in {v}"))?;
        let out: Value = serde_json::from_str(text).unwrap_or(Value::String(text.to_string()));
        if out.get("ok") == Some(&Value::Bool(false)) {
            return Err(format!("{tool}: {}", out.get("error").and_then(Value::as_str).unwrap_or("failed")));
        }
        Ok(out)
    }

    /// Read a resource and parse its JSON text; the envelope's `data` when there is one.
    pub fn read(&self, uri: &str) -> Result<Value, String> {
        let v = self.rpc("resources/read", json!({"uri": uri}))?;
        let text = v.pointer("/result/contents/0/text").and_then(Value::as_str).ok_or_else(|| format!("{uri}: no contents in {v}"))?;
        let out: Value = serde_json::from_str(text).map_err(|e| format!("{uri}: {e}"))?;
        Ok(out.get("data").cloned().unwrap_or(out))
    }

    /// The tool names the process offers right now (tools/list).
    pub fn tools(&self) -> Result<Vec<String>, String> {
        let v = self.rpc("tools/list", json!({}))?;
        Ok(v.pointer("/result/tools")
            .and_then(Value::as_array)
            .map(|a| a.iter().filter_map(|t| t.get("name").and_then(Value::as_str).map(str::to_string)).collect())
            .unwrap_or_default())
    }
}
