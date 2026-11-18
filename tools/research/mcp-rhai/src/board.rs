//! RESEARCH-ONLY. A client of the Majordomus board (ADR 0176 there): one MCP session over HTTP
//! (`POST /mcp`, JSON answers, no stream) on a checkout's Majordomus server, through which a process
//! that is not itself an MCP client of the board announces named claims (`majordomus_announce`).
//! The board keeps a departed peer's named claims, so the last post stays readable after the
//! process stops. Writes nothing to the repository.
//!
//! Users: `mcp-bus-blackboard` (the panels' bus digest) and `mcp-rhai`'s `board_post(claim, text)`
//! (the rail monitor, tools/research/mcp-panel/rhai/rail_monitor.rhai).

use serde_json::{json, Value};
use std::collections::BTreeMap;
use std::time::{Duration, Instant};

/// How often a quiet session is pinged: the server forgets a session silent for 90 s.
pub const PING: Duration = Duration::from_secs(30);

/// Cut a line to `max` characters, marking the cut.
pub fn clip(s: &str, max: usize) -> String {
    if s.chars().count() <= max {
        return s.to_string();
    }
    let mut out: String = s.chars().take(max.saturating_sub(1)).collect();
    out.push('…');
    out
}

/// The primary checkout of the repository around `dir`: the parent of git's common directory.
pub fn primary_checkout(dir: Option<&str>) -> Option<String> {
    let mut cmd = std::process::Command::new("git");
    if let Some(d) = dir { cmd.arg("-C").arg(d); }
    let out = cmd.args(["rev-parse", "--path-format=absolute", "--git-common-dir"]).output().ok()?;
    let common = String::from_utf8(out.stdout).ok()?;
    std::path::Path::new(common.trim()).parent().map(|p| p.display().to_string())
}

/// The Majordomus server: `explicit` when given, else `$MAJORDOMUS_URL`, else the one
/// `majordomus serve ensure` names for the primary checkout of `repo` (default: the current
/// directory's repository).
pub fn majordomus_url(explicit: Option<&str>, repo: Option<&str>) -> Result<String, String> {
    if let Some(u) = explicit.filter(|u| !u.is_empty()) { return Ok(u.to_string()); }
    if let Ok(u) = std::env::var("MAJORDOMUS_URL") { if !u.is_empty() { return Ok(u); } }
    let repo = primary_checkout(repo).or_else(|| repo.map(str::to_string)).unwrap_or_else(|| ".".into());
    let out = std::process::Command::new("majordomus").args(["serve", "ensure", "--format", "json", "--repo", &repo])
        .stderr(std::process::Stdio::null()).output().map_err(|e| format!("majordomus serve ensure: {e}"))?;
    let v: Value = serde_json::from_slice(&out.stdout).map_err(|e| format!("majordomus serve ensure: {e}"))?;
    v["url"].as_str().map(str::to_string).ok_or(format!("majordomus serve ensure ({repo}): no url ({})", v["standing"]))
}

/// An MCP session on a Majordomus server's board.
pub struct Board {
    url: String,
    session: Option<String>,
    dry: bool,
    /// The MCP clientInfo this session names itself with: (name, title).
    client: (String, String),
    /// Every claim announced, so a re-initialised session says them again.
    claims: BTreeMap<String, String>,
    next_id: u64,
    last_rpc: Instant,
}

impl Board {
    /// A session to the board at `url`, opened on the first announcement. `dry` prints instead.
    pub fn new(url: &str, dry: bool, name: &str, title: &str) -> Self {
        Board { url: url.trim_end_matches('/').to_string(), session: None, dry, client: (name.to_string(), title.to_string()),
                claims: BTreeMap::new(), next_id: 1, last_rpc: Instant::now() }
    }

    fn post(&mut self, body: &Value) -> Result<Option<Value>, ureq::Error> {
        let mut req = ureq::post(&format!("{}/mcp", self.url)).timeout(Duration::from_secs(10))
            .set("Content-Type", "application/json").set("Accept", "application/json, text/event-stream");
        if let Some(s) = &self.session { req = req.set("Mcp-Session-Id", s); }
        let resp = req.send_json(body.clone())?;
        self.last_rpc = Instant::now();
        if let Some(s) = resp.header("Mcp-Session-Id") { self.session = Some(s.to_string()); }
        if resp.status() == 202 || resp.status() == 204 { return Ok(None); }
        Ok(resp.into_json::<Value>().ok())
    }

    fn initialize(&mut self) -> Result<(), String> {
        self.session = None;
        let init = json!({"jsonrpc": "2.0", "id": 0, "method": "initialize", "params": {
            "protocolVersion": "2025-06-18", "capabilities": {},
            "clientInfo": {"name": self.client.0, "title": self.client.1, "version": env!("CARGO_PKG_VERSION")}}});
        self.post(&init).map_err(|e| format!("{}/mcp initialize: {e}", self.url))?;
        if self.session.is_none() { return Err(format!("{}/mcp: no Mcp-Session-Id in the answer", self.url)); }
        let _ = self.post(&json!({"jsonrpc": "2.0", "method": "notifications/initialized"}));
        Ok(())
    }

    /// One JSON-RPC call; a forgotten session (404) is opened again, its claims said again, and the
    /// call retried once.
    fn rpc(&mut self, method: &str, params: Value) -> Result<Value, String> {
        for attempt in 0..2 {
            if self.session.is_none() {
                self.initialize()?;
                let replay: Vec<(String, String)> = self.claims.iter().map(|(k, v)| (k.clone(), v.clone())).collect();
                for (claim, intent) in replay { self.call_announce(&claim, &intent)?; }
            }
            self.next_id += 1;
            let body = json!({"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params});
            match self.post(&body) {
                Ok(v) => {
                    let v = v.unwrap_or(Value::Null);
                    if let Some(e) = v.get("error") { return Err(format!("{method}: {e}")); }
                    return Ok(v["result"].clone());
                }
                Err(ureq::Error::Status(404, _)) if attempt == 0 => self.session = None,
                Err(e) => return Err(format!("{}/mcp {method}: {e}", self.url)),
            }
        }
        Err(format!("{method}: the session was forgotten twice"))
    }

    fn call_announce(&mut self, claim: &str, intent: &str) -> Result<(), String> {
        let mut args = json!({"intent": intent, "scope": []});
        if !claim.is_empty() { args["claim"] = json!(claim); }
        self.next_id += 1;
        let body = json!({"jsonrpc": "2.0", "id": self.next_id, "method": "tools/call", "params": {"name": "majordomus_announce", "arguments": args}});
        let v = self.post(&body).map_err(|e| format!("announce {claim}: {e}"))?.unwrap_or(Value::Null);
        if v.get("error").is_some() || v.pointer("/result/isError") == Some(&json!(true)) {
            return Err(format!("announce {claim}: {}", clip(&v.to_string(), 300)));
        }
        Ok(())
    }

    /// Announce one claim (`""` is this session's unnamed one).
    pub fn announce(&mut self, claim: &str, intent: &str) -> Result<(), String> {
        self.claims.insert(claim.to_string(), intent.to_string());
        if self.dry {
            println!("announce {}: {intent}", if claim.is_empty() { "(unnamed)" } else { claim });
            return Ok(());
        }
        if self.session.is_none() {
            return self.rpc("ping", json!({})).map(|_| ());
        }
        match self.call_announce(claim, intent) {
            Err(e) if e.contains("404") => { self.session = None; self.rpc("ping", json!({})).map(|_| ()) }
            other => other,
        }
    }

    /// Ping a session quiet for PING, so the server does not forget it between posts.
    pub fn keep_alive(&mut self) {
        if !self.dry && self.session.is_some() && self.last_rpc.elapsed() >= PING {
            if let Err(e) = self.rpc("ping", json!({})) { eprintln!("{}: {e}", self.client.0); }
        }
    }

    /// End the session (the board keeps its named claims).
    pub fn close(&mut self) {
        if let (false, Some(s)) = (self.dry, self.session.take()) {
            let _ = ureq::delete(&format!("{}/mcp", self.url)).timeout(Duration::from_secs(5)).set("Mcp-Session-Id", &s).call();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clip_marks_the_cut() {
        assert_eq!(clip("abc", 3), "abc");
        assert_eq!(clip("abcdef", 4), "abc…");
    }

    #[test]
    fn an_explicit_url_wins() {
        assert_eq!(majordomus_url(Some("http://127.0.0.1:1"), None).unwrap(), "http://127.0.0.1:1");
    }

    #[test]
    fn a_dry_board_announces_without_a_server() {
        let mut b = Board::new("http://127.0.0.1:9", true, "test", "Test");
        assert!(b.announce("openttd:x:rail", "a line").is_ok());
        b.keep_alive();
        b.close();
    }
}
