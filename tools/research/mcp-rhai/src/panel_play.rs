//! RESEARCH-ONLY: play the running game in the browser (research/play-in-browser.md).
//!
//! The browser runs the real OpenTTD client compiled to WebAssembly (upstream's Emscripten port,
//! os/emscripten) and draws every frame itself. It joins the dedicated server through the normal
//! lockstep netcode, so only network packets cross the wire, never pictures. Emscripten turns the
//! client's TCP socket into a WebSocket (SOCKFS), and this module is the other end:
//!
//! - `GET /ws/game` (WebSocket upgrade): one bridge per WebSocket to `127.0.0.1:<game port>`. The
//!   target is fixed by the panel, never chosen by the client, so it is no open proxy. The handshake
//!   refuses a foreign Origin (only the panel's own pages may open it) and echoes the `binary`
//!   subprotocol Emscripten asks for. Frames are raw TCP bytes both ways; OpenTTD packets carry their
//!   own lengths. At most `MAX_BRIDGES` at once.
//! - `GET /play/<file>`: the built client (`openttd.js`, `.wasm`, `.data`) from the WASM build
//!   directory ($MCP_PLAY_DIR, else `build-wasm` at the repository root), `application/wasm` for
//!   streaming compilation.
//!
//! Every change the browser's player makes is a command the server executes like any client's
//! (src/command.cpp), so the command system and determinism are untouched. The panel listens on
//! loopback only.
use std::io::{ErrorKind, Read, Write};
use std::net::{Shutdown, TcpStream};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::Duration;
use tungstenite::handshake::server::{ErrorResponse, Request, Response};
use tungstenite::{Message, WebSocket};

/// Bridges open at once: a browser tab is one, so a handful is plenty and a runaway page is capped.
pub const MAX_BRIDGES: usize = 4;
/// The largest WebSocket message accepted from the browser.
pub const MAX_MESSAGE: usize = 1 << 20;
static OPEN: AtomicUsize = AtomicUsize::new(0);

/// Whether a connection is a WebSocket upgrade for the game bridge, without consuming its bytes.
pub fn is_game_upgrade(stream: &TcpStream) -> bool {
    let mut head = [0u8; 13];
    matches!(stream.peek(&mut head), Ok(n) if head[..n].starts_with(b"GET /ws/game"))
}

/// Whether an Origin header may open the bridge: none (a non-browser client on loopback) or the
/// panel's own address, as 127.0.0.1 or localhost.
pub fn origin_allowed(origin: Option<&str>, panel_port: u16) -> bool {
    match origin {
        None => true,
        Some(o) => o == format!("http://127.0.0.1:{panel_port}") || o == format!("http://localhost:{panel_port}"),
    }
}

/// The WASM build's directory: $MCP_PLAY_DIR, else `<repo>/build-wasm`.
pub fn play_dir(repo: &Path) -> PathBuf {
    std::env::var_os("MCP_PLAY_DIR").map(PathBuf::from).unwrap_or_else(|| repo.join("build-wasm"))
}

/// A file of the built client and its type, or None for anything else (no path steps, no other
/// files of the build directory).
pub fn play_file(dir: &Path, name: &str) -> Option<(PathBuf, &'static str)> {
    let ctype = match name {
        "openttd.js" => "text/javascript",
        "openttd.wasm" => "application/wasm",
        "openttd.data" => "application/octet-stream",
        _ => return None,
    };
    let f = dir.join(name);
    f.is_file().then_some((f, ctype))
}

/// Whether the client is built (the page says what to run when it is not).
pub fn play_built(dir: &Path) -> bool {
    ["openttd.js", "openttd.wasm", "openttd.data"].iter().all(|f| dir.join(f).is_file())
}

/// Accept the upgrade and pump bytes between the WebSocket and the game server until either side
/// closes. Blocks the connection's thread.
pub fn bridge(stream: TcpStream, panel_port: u16, game_port: u16) {
    if OPEN.fetch_add(1, Ordering::SeqCst) >= MAX_BRIDGES {
        OPEN.fetch_sub(1, Ordering::SeqCst);
        let _ = (&stream).write_all(b"HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        return;
    }
    run(stream, panel_port, game_port);
    OPEN.fetch_sub(1, Ordering::SeqCst);
}

fn run(stream: TcpStream, panel_port: u16, game_port: u16) {
    let _ = stream.set_nodelay(true);
    let check = |req: &Request, mut resp: Response| -> Result<Response, ErrorResponse> {
        let origin = req.headers().get("Origin").and_then(|v| v.to_str().ok());
        if !origin_allowed(origin, panel_port) {
            let mut e = ErrorResponse::new(Some("foreign origin".into()));
            *e.status_mut() = tungstenite::http::StatusCode::FORBIDDEN;
            return Err(e);
        }
        // Emscripten's SOCKFS asks for "binary"; a browser drops a socket whose protocol is not echoed.
        let wants_binary = req.headers().get("Sec-WebSocket-Protocol").and_then(|v| v.to_str().ok())
            .is_some_and(|p| p.split(',').any(|s| s.trim() == "binary"));
        if wants_binary {
            resp.headers_mut().insert("Sec-WebSocket-Protocol", "binary".parse().unwrap());
        }
        Ok(resp)
    };
    let mut cfg = tungstenite::protocol::WebSocketConfig::default();
    cfg.max_message_size = Some(MAX_MESSAGE);
    cfg.max_frame_size = Some(MAX_MESSAGE);
    let Ok(mut ws) = tungstenite::accept_hdr_with_config(stream, check, Some(cfg)) else { return };
    let Ok(mut game) = TcpStream::connect(("127.0.0.1", game_port)) else {
        let _ = ws.close(None);
        return;
    };
    let _ = game.set_nodelay(true);
    pump(&mut ws, &mut game);
    let _ = game.shutdown(Shutdown::Both);
    let _ = ws.close(None);
    let _ = ws.flush();
}

/// One thread, two directions: short read timeouts on both sockets, so neither direction waits on
/// the other for longer than a few milliseconds (a game tick is 27 ms).
fn pump(ws: &mut WebSocket<TcpStream>, game: &mut TcpStream) {
    let _ = ws.get_mut().set_read_timeout(Some(Duration::from_millis(2)));
    let _ = game.set_read_timeout(Some(Duration::from_millis(2)));
    let mut buf = vec![0u8; 64 * 1024];
    loop {
        match ws.read() {
            Ok(Message::Binary(b)) => { if game.write_all(&b).is_err() { return; } }
            Ok(Message::Text(t)) => { if game.write_all(t.as_bytes()).is_err() { return; } }
            Ok(Message::Close(_)) => return,
            Ok(_) => {}
            Err(tungstenite::Error::Io(e)) if matches!(e.kind(), ErrorKind::WouldBlock | ErrorKind::TimedOut) => {}
            Err(_) => return,
        }
        match game.read(&mut buf) {
            Ok(0) => return,
            Ok(n) => { if ws.send(Message::binary(buf[..n].to_vec())).is_err() { return; } }
            Err(e) if matches!(e.kind(), ErrorKind::WouldBlock | ErrorKind::TimedOut) => {}
            Err(_) => return,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::TcpListener;

    #[test]
    fn only_the_panels_own_pages_may_open_the_bridge() {
        assert!(origin_allowed(None, 8800));
        assert!(origin_allowed(Some("http://127.0.0.1:8800"), 8800));
        assert!(origin_allowed(Some("http://localhost:8800"), 8800));
        assert!(!origin_allowed(Some("http://127.0.0.1:9999"), 8800));
        assert!(!origin_allowed(Some("https://evil.example"), 8800));
        assert!(!origin_allowed(Some("http://127.0.0.1:8800.evil.example"), 8800));
    }

    #[test]
    fn only_the_built_client_is_served() {
        let d = std::env::temp_dir().join(format!("play-{}", std::process::id()));
        std::fs::create_dir_all(&d).unwrap();
        for f in ["openttd.js", "openttd.wasm", "openttd.data", "CMakeCache.txt"] { std::fs::write(d.join(f), b"x").unwrap(); }
        assert_eq!(play_file(&d, "openttd.wasm").unwrap().1, "application/wasm");
        assert!(play_file(&d, "CMakeCache.txt").is_none());
        assert!(play_file(&d, "../openttd.js").is_none());
        assert!(play_built(&d));
        std::fs::remove_file(d.join("openttd.data")).unwrap();
        assert!(!play_built(&d));
        let _ = std::fs::remove_dir_all(&d);
    }

    /// A WebSocket client through the bridge reaches an echo "game server": bytes go both ways,
    /// the binary subprotocol is echoed, and a foreign Origin is refused.
    #[test]
    fn bytes_round_trip_through_the_bridge() {
        let game = TcpListener::bind("127.0.0.1:0").unwrap();
        let game_port = game.local_addr().unwrap().port();
        std::thread::spawn(move || {
            for s in game.incoming().flatten() {
                std::thread::spawn(move || {
                    let mut s = s;
                    let mut b = [0u8; 1024];
                    while let Ok(n) = s.read(&mut b) { if n == 0 || s.write_all(&b[..n]).is_err() { break; } }
                });
            }
        });
        let panel = TcpListener::bind("127.0.0.1:0").unwrap();
        let panel_port = panel.local_addr().unwrap().port();
        std::thread::spawn(move || {
            for s in panel.incoming().flatten() {
                std::thread::spawn(move || { assert!(is_game_upgrade(&s)); bridge(s, panel_port, game_port) });
            }
        });
        let url = format!("ws://127.0.0.1:{panel_port}/ws/game");
        let mut req = tungstenite::client::IntoClientRequest::into_client_request(url.as_str()).unwrap();
        req.headers_mut().insert("Sec-WebSocket-Protocol", "binary".parse().unwrap());
        req.headers_mut().insert("Origin", format!("http://127.0.0.1:{panel_port}").parse().unwrap());
        let tcp = TcpStream::connect(("127.0.0.1", panel_port)).unwrap();
        let (mut ws, resp) = tungstenite::client(req, tcp).unwrap();
        assert_eq!(resp.headers().get("Sec-WebSocket-Protocol").unwrap(), "binary");
        ws.send(Message::binary(b"\x05\x00\x00hi".to_vec())).unwrap();
        let mut got = Vec::new();
        while got.len() < 5 {
            if let Message::Binary(b) = ws.read().unwrap() { got.extend_from_slice(&b); }
        }
        assert_eq!(got, b"\x05\x00\x00hi");
        let _ = ws.close(None);

        let mut req = tungstenite::client::IntoClientRequest::into_client_request(url.as_str()).unwrap();
        req.headers_mut().insert("Origin", "https://evil.example".parse().unwrap());
        let tcp = TcpStream::connect(("127.0.0.1", panel_port)).unwrap();
        assert!(tungstenite::client(req, tcp).is_err(), "a foreign origin is refused");
    }
}
