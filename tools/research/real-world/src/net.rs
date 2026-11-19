//! The only place that touches the network, and only when a command was given `--live`.
//!
//! Policy (re-checked 2026-11-18, see research/real-world-scenario.md "Sources and terms"):
//! - a generic identifying User-Agent, never a person's name or address;
//! - at most one request per second per host (Nominatim's absolute maximum), single thread;
//! - every response cached on disk by the SHA-256 of its request, and a cached answer is
//!   reused instead of asking again (Nominatim and Overpass both require caching);
//! - timeouts on connect and read, at most two retries, and a 30 s pause after HTTP 429
//!   (Overpass's documented back-off); a layer that still fails is reported partial, not hidden.

use sha2::{Digest, Sha256};
#[cfg(feature = "live")]
use std::collections::BTreeMap;
#[cfg(feature = "live")]
use std::path::PathBuf;
#[cfg(feature = "live")]
use std::time::{Duration, Instant};

/// Generic and identifying; it names no person (the only project URL carries the owner's handle,
/// which the brief keeps out of every request).
pub const USER_AGENT: &str = "openttd-real-world-research/0.1 (research fork; rw-scenario)";
#[cfg(feature = "live")]
const MIN_INTERVAL: Duration = Duration::from_millis(1100);
#[cfg(feature = "live")]
const MAX_RETRIES: u32 = 2;

pub fn sha256_hex(bytes: &[u8]) -> String {
    let mut h = Sha256::new();
    h.update(bytes);
    h.finalize().iter().map(|b| format!("{b:02x}")).collect()
}

/// One answer: its bytes, the SHA-256 of them and whether it came from the cache.
#[cfg(feature = "live")]
pub struct Fetched {
    pub body: Vec<u8>,
    pub sha256: String,
    pub cached: bool,
    /// When the answer came from the network (kept beside a cached answer).
    pub fetched_at: String,
}

#[cfg(feature = "live")]
pub struct Client {
    agent: ureq::Agent,
    cache: PathBuf,
    last: BTreeMap<String, Instant>,
    /// Requests that reached the network, per host.
    pub sent: BTreeMap<String, u32>,
    /// Retries and back-offs, for the source's diagnostics.
    pub notes: Vec<String>,
}

#[cfg(feature = "live")]
impl Client {
    pub fn new(cache: PathBuf) -> Self {
        let agent = ureq::AgentBuilder::new()
            .user_agent(USER_AGENT)
            .timeout_connect(Duration::from_secs(15))
            .timeout_read(Duration::from_secs(120))
            .build();
        Client { agent, cache, last: BTreeMap::new(), sent: BTreeMap::new(), notes: Vec::new() }
    }

    fn host(url: &str) -> String {
        url.split("://").nth(1).unwrap_or(url).split('/').next().unwrap_or("").to_string()
    }

    fn pace(&mut self, host: &str) {
        if let Some(t) = self.last.get(host) {
            let since = t.elapsed();
            if since < MIN_INTERVAL {
                std::thread::sleep(MIN_INTERVAL - since);
            }
        }
        self.last.insert(host.to_string(), Instant::now());
    }

    /// GET `url`, or POST `form` (application/x-www-form-urlencoded) when it is given.
    pub fn fetch(&mut self, url: &str, form: Option<&[(&str, &str)]>) -> Result<Fetched, String> {
        self.fetch_with(url, form, None)
    }

    /// GET bytes `start..=end` of `url` (an HTTP range request; the server must answer 206 with
    /// exactly that many bytes). Only the window asked for is read and cached, never the whole file.
    pub fn fetch_range(&mut self, url: &str, start: u64, end: u64) -> Result<Fetched, String> {
        let f = self.fetch_with(url, None, Some((start, end)))?;
        if f.body.len() as u64 != end - start + 1 {
            return Err(format!("{url}: range {start}-{end} answered {} bytes", f.body.len()));
        }
        Ok(f)
    }

    fn fetch_with(&mut self, url: &str, form: Option<&[(&str, &str)]>, range: Option<(u64, u64)>) -> Result<Fetched, String> {
        // The key of a plain request is unchanged by range support, so earlier caches stay valid.
        let key = match range {
            None => sha256_hex(format!("{url}\n{form:?}").as_bytes()),
            Some((a, b)) => sha256_hex(format!("{url}\n{form:?}\nrange={a}-{b}").as_bytes()),
        };
        let path = self.cache.join(format!("{key}.body"));
        let at_path = self.cache.join(format!("{key}.at"));
        if let Ok(body) = std::fs::read(&path) {
            let sha256 = sha256_hex(&body);
            let fetched_at = std::fs::read_to_string(&at_path).unwrap_or_default().trim().to_string();
            return Ok(Fetched { body, sha256, cached: true, fetched_at });
        }
        let host = Self::host(url);
        let mut attempt = 0;
        loop {
            self.pace(&host);
            *self.sent.entry(host.clone()).or_insert(0) += 1;
            let res = match (form, range) {
                (Some(f), _) => self.agent.post(url).send_form(f),
                (None, Some((a, b))) => self.agent.get(url).set("Range", &format!("bytes={a}-{b}")).call(),
                (None, None) => self.agent.get(url).call(),
            };
            match res {
                Ok(r) => {
                    if range.is_some() && r.status() != 206 {
                        return Err(format!("{host}: range request answered HTTP {} (206 expected)", r.status()));
                    }
                    let mut body = Vec::new();
                    std::io::Read::read_to_end(&mut r.into_reader(), &mut body).map_err(|e| format!("{host}: read: {e}"))?;
                    std::fs::create_dir_all(&self.cache).map_err(|e| format!("cache: {e}"))?;
                    std::fs::write(&path, &body).map_err(|e| format!("cache: {e}"))?;
                    let fetched_at = now_utc();
                    std::fs::write(&at_path, &fetched_at).map_err(|e| format!("cache: {e}"))?;
                    let sha256 = sha256_hex(&body);
                    return Ok(Fetched { body, sha256, cached: false, fetched_at });
                }
                Err(ureq::Error::Status(code, _)) if attempt < MAX_RETRIES && (code == 429 || code >= 500) => {
                    attempt += 1;
                    let wait = if code == 429 { 30 } else { 5 * attempt as u64 };
                    eprintln!("rw-scenario: {host} answered {code}; retry {attempt}/{MAX_RETRIES} in {wait} s");
                    self.notes.push(format!("{host} answered HTTP {code}; retried after {wait} s ({attempt}/{MAX_RETRIES})"));
                    std::thread::sleep(Duration::from_secs(wait));
                }
                Err(ureq::Error::Status(code, _)) => return Err(format!("{host}: HTTP {code}")),
                Err(e) if attempt < MAX_RETRIES => {
                    attempt += 1;
                    eprintln!("rw-scenario: {host}: {e}; retry {attempt}/{MAX_RETRIES}");
                    std::thread::sleep(Duration::from_secs(5));
                }
                Err(e) => return Err(format!("{host}: {e}")),
            }
        }
    }
}

/// Percent-encode a query component (RFC 3986 unreserved characters pass).
pub fn encode(s: &str) -> String {
    let mut out = String::new();
    for b in s.bytes() {
        match b {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => out.push(b as char),
            _ => out.push_str(&format!("%{b:02X}")),
        }
    }
    out
}

/// The current UTC time as ISO 8601, for `retrieved_at` only (acquisition, never compilation).
pub fn now_utc() -> String {
    let secs = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    let days = (secs / 86_400) as i64;
    let rem = secs % 86_400;
    // Civil-from-days (Howard Hinnant), proleptic Gregorian.
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = if m <= 2 { y + 1 } else { y };
    format!("{y:04}-{m:02}-{d:02}T{:02}:{:02}:{:02}Z", rem / 3600, (rem % 3600) / 60, rem % 60)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_user_agent_names_no_person() {
        assert!(!USER_AGENT.contains('@'));
        assert!(USER_AGENT.starts_with("openttd-real-world-research/"));
    }

    #[test]
    fn encoding_and_hashing_are_stable() {
        assert_eq!(encode("Brno, Czech Republic"), "Brno%2C%20Czech%20Republic");
        assert_eq!(sha256_hex(b"abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        assert_eq!(now_utc().len(), 20);
    }
}
