//! `load-check`: prove a compiled scenario loads through the supported path, headless.
//!
//! It starts a dedicated server (`-D 127.0.0.1:<free port>`, loopback only) on the compiled
//! heightmap (`-g heightmap.png`, a supported start-up path: `src/openttd.cpp`, case 'g') with the
//! scenario's settings and its GameScript, in a personal directory of its own (`HOME` points into
//! the work directory, so the user's OpenTTD directory is never written; only the base sets are
//! linked read-only). It reads the GameScript's `rw:` log lines, saves the game from the server
//! console, quits, and writes `loadcheck.json`. It stops only the process it started.

use serde_json::{json, Value};
use std::io::{BufRead, Write};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::mpsc;
use std::time::{Duration, Instant};

pub struct Options {
    pub out: PathBuf,
    pub openttd: PathBuf,
    pub baseset: Option<PathBuf>,
    pub seed: u32,
    pub timeout: Duration,
}

fn free_port() -> Result<u16, String> {
    let l = std::net::TcpListener::bind("127.0.0.1:0").map_err(|e| format!("port: {e}"))?;
    Ok(l.local_addr().map_err(|e| e.to_string())?.port())
}

fn copy_dir(from: &Path, to: &Path) -> Result<(), String> {
    std::fs::create_dir_all(to).map_err(|e| format!("{}: {e}", to.display()))?;
    for e in std::fs::read_dir(from).map_err(|e| format!("{}: {e}", from.display()))? {
        let e = e.map_err(|e| e.to_string())?;
        std::fs::copy(e.path(), to.join(e.file_name())).map_err(|e| e.to_string())?;
    }
    Ok(())
}

/// A town or industry placed further than this (Manhattan tiles) from its compiled tile is flagged.
pub const DISPLACEMENT_THRESHOLD: u32 = 8;

/// Returns the report and the exit code: 0 everything placed, saved and reloaded unchanged; 6 loaded
/// but placement partial, an entity without its in-game note, or the reload changed the counts;
/// 7 the game did not load or the GameScript did not finish.
pub fn run(o: &Options) -> Result<(Value, i32), String> {
    let scenario: Value = serde_json::from_slice(&std::fs::read(o.out.join("scenario.json")).map_err(|e| format!("scenario.json: {e}"))?)
        .map_err(|e| format!("scenario.json: {e}"))?;
    let out = std::fs::canonicalize(&o.out).map_err(|e| format!("{}: {e}", o.out.display()))?;
    let work = out.join("loadcheck");
    if work.exists() {
        std::fs::remove_dir_all(&work).map_err(|e| format!("{}: {e}", work.display()))?;
    }
    let pd = work.join("home/Documents/OpenTTD");
    std::fs::create_dir_all(pd.join("save")).map_err(|e| e.to_string())?;
    // The GameScript and the base set go both into HOME's personal directory (where macOS looks)
    // and into the config file's directory, which -c makes the personal directory on every
    // platform (on Linux the HOME one is ~/.local/share/openttd, not Documents/OpenTTD).
    for dir in [pd.clone(), work.clone()] {
        copy_dir(&out.join("game").join(crate::gamescript::NAME), &dir.join("game").join(crate::gamescript::NAME))?;
    }
    let mut baseset_note = "none linked: the binary's own baseset directory is used".to_string();
    if let Some(b) = &o.baseset {
        if b.is_dir() {
            for dir in [&pd, &work] {
                std::os::unix::fs::symlink(b, dir.join("baseset")).map_err(|e| format!("baseset link: {e}"))?;
            }
            baseset_note = format!("linked read-only from {}", b.display());
        }
    }
    let port = free_port()?;
    // One header per section: OpenTTD's ini reader keeps only the first section of a name.
    let mut sections: std::collections::BTreeMap<String, Vec<String>> = std::collections::BTreeMap::new();
    for (k, v) in scenario["settings"].as_object().ok_or("scenario.json: no settings")? {
        let (sec, key) = k.split_once('.').unwrap_or(("misc", k));
        sections.entry(sec.to_string()).or_default().push(format!("{key} = {}", v.as_str().unwrap_or("")));
    }
    let mut cfg = String::new();
    for (sec, lines) in &sections {
        cfg.push_str(&format!("[{sec}]\n{}\n", lines.join("\n")));
    }
    cfg.push_str(&format!(
        "[game_scripts]\n{} = \n[gui]\nautosave = off\n[network]\nserver_port = {port}\nserver_game_type = local\n",
        crate::gamescript::NAME
    ));
    let cfg_path = work.join("openttd.cfg");
    std::fs::write(&cfg_path, &cfg).map_err(|e| e.to_string())?;
    let hm = std::fs::canonicalize(out.join("heightmap.png")).map_err(|e| format!("heightmap.png: {e}"))?;
    let args: Vec<String> = vec![
        "-D".into(),
        format!("127.0.0.1:{port}"),
        "-g".into(),
        hm.display().to_string(),
        "-c".into(),
        cfg_path.display().to_string(),
        "-x".into(),
        "-G".into(),
        o.seed.to_string(),
        "-d".into(),
        "script=4".into(),
    ];
    let home = std::fs::canonicalize(work.join("home")).map_err(|e| e.to_string())?;
    let mut child = Command::new(&o.openttd)
        .args(&args)
        .env("HOME", &home)
        .current_dir(&work)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("{}: {e}", o.openttd.display()))?;
    let pid = child.id();
    let (tx, rx) = mpsc::channel::<String>();
    for stream in [child.stdout.take().map(|s| Box::new(s) as Box<dyn std::io::Read + Send>), child.stderr.take().map(|s| Box::new(s) as Box<dyn std::io::Read + Send>)].into_iter().flatten() {
        let tx = tx.clone();
        std::thread::spawn(move || {
            for line in std::io::BufReader::new(stream).lines().map_while(Result::ok) {
                if tx.send(line).is_err() {
                    break;
                }
            }
        });
    }
    drop(tx);
    let mut stdin = child.stdin.take();
    let mut send = |line: &str| {
        if let Some(s) = stdin.as_mut() {
            let _ = writeln!(s, "{line}");
            let _ = s.flush();
        }
    };
    let start = Instant::now();
    let mut log = Vec::new();
    // Phases: place (the GameScript runs) -> save -> reload the save -> quit. The reload proves a
    // saved scenario does not place everything a second time.
    let (mut done, mut saved, mut resumed, mut died) = (false, false, false, false);
    let (mut sent_save, mut sent_load, mut sent_quit) = (false, false, false);
    let mut load_sent_at: Option<Instant> = None;
    let save_name = "rw-loadcheck";
    while start.elapsed() < o.timeout {
        match rx.recv_timeout(Duration::from_millis(250)) {
            Ok(l) => {
                done |= l.contains("rw: done");
                resumed |= l.contains("rw: resumed");
                saved |= l.contains("Map successfully saved");
                // A script that does not compile or dies will not finish: stop waiting for it.
                died |= l.contains("The script died") || l.contains("Failed to compile");
                log.push(l);
            }
            Err(mpsc::RecvTimeoutError::Disconnected) => break,
            Err(mpsc::RecvTimeoutError::Timeout) => {}
        }
        let save_file = work.join("save").join(format!("{save_name}.sav"));
        if died && !sent_quit {
            send("quit");
            sent_quit = true;
        }
        if done && !sent_save {
            send(&format!("save {save_name}"));
            sent_save = true;
        }
        if sent_save && !sent_load && (saved || save_file.exists()) {
            std::thread::sleep(Duration::from_millis(500));
            send(&format!("load {save_name}"));
            sent_load = true;
            load_sent_at = Some(Instant::now());
        }
        let reload_waited = load_sent_at.map(|t| t.elapsed() > Duration::from_secs(90)).unwrap_or(false);
        if sent_load && !sent_quit && (resumed || reload_waited) {
            std::thread::sleep(Duration::from_millis(500));
            send("quit");
            sent_quit = true;
        }
        if let Ok(Some(_)) = child.try_wait() {
            // Drain what is left.
            while let Ok(l) = rx.recv_timeout(Duration::from_millis(200)) {
                log.push(l);
            }
            break;
        }
    }
    let mut killed = false;
    let status = match child.try_wait() {
        Ok(Some(s)) => s.code(),
        _ => {
            let deadline = Instant::now() + Duration::from_secs(15);
            let mut st = None;
            while Instant::now() < deadline {
                if let Ok(Some(s)) = child.try_wait() {
                    st = s.code();
                    break;
                }
                std::thread::sleep(Duration::from_millis(200));
            }
            if st.is_none() {
                let _ = child.kill();
                let _ = child.wait();
                killed = true;
            }
            st
        }
    };
    std::fs::write(work.join("openttd.log"), log.join("\n") + "\n").map_err(|e| e.to_string())?;
    let rw: Vec<&String> = log.iter().filter(|l| l.contains("rw: ")).collect();
    let count = |p: &str| rw.iter().filter(|l| l.contains(p)).count();
    let done_line = rw.iter().find(|l| l.contains("rw: done")).map(|l| l.split("rw: ").nth(1).unwrap_or("").to_string());
    // With -c, OpenTTD makes the config file's directory the personal directory, so the save lands
    // in <work>/save; the HOME one is checked too.
    let save_file = [work.join("save"), pd.join("save")].iter().map(|d| d.join(format!("{save_name}.sav"))).find(|p| p.is_file()).unwrap_or_else(|| work.join("save").join(format!("{save_name}.sav")));
    let save = std::fs::read(&save_file).ok();
    let want = |k: &str| scenario["entities"].as_array().map(|a| a.iter().filter(|e| e["game"]["kind"] == k).count()).unwrap_or(0);
    let (wt, wi, ws) = (want("town"), want("industry"), want("sign"));
    let (ot, oi, os) = (count("rw: town ok"), count("rw: industry ok"), count("rw: sign ok"));
    // Every placed town and industry must carry its evidence note in the game (signs are labels).
    let labelled = rw.iter().filter(|l| (l.contains("rw: town ok") || l.contains("rw: industry ok")) && l.contains("labelled=true")).count();
    let unlabelled: Vec<String> = rw.iter().filter(|l| (l.contains("rw: town ok") || l.contains("rw: industry ok")) && !l.contains("labelled=true"))
        .map(|l| l.split("rw: ").nth(1).unwrap_or("").split_whitespace().take(3).collect::<Vec<_>>().join(" ")).collect();
    let after = |l: &str, key: &str| -> Option<String> { l.split(key).nth(1).map(|r| r.split_whitespace().next().unwrap_or("").to_string()) };
    let between = |l: &str, a: &str, b: &str| -> Option<String> { let r = l.split(a).nth(1)?; Some(r.split(b).next()?.to_string()) };
    let pair = |v: Option<String>| -> Option<(u32, u32)> { let v = v?; let (a, b) = v.split_once(',')?; Some((a.parse().ok()?, b.parse().ok()?)) };
    // Where each town and industry ended up, and how far from its compiled tile.
    let mut placements = Vec::new();
    let mut displaced = Vec::new();
    // A placement line that does not parse is a failure, never a silent skip: a skipped line once
    // hid an industry placed across the map edge.
    let mut unparsed = Vec::new();
    // Placed outside the anchor's mapped extent (its bounding box plus one tile), whatever its size.
    let mut outside_site = Vec::new();
    for l in rw.iter().filter(|l| l.contains("rw: town ok") || l.contains("rw: industry ok")) {
        let id = after(l, " ok ").unwrap_or_default();
        let (Some((x, y)), Some((dx, dy))) = (pair(after(l, " at ")), pair(after(l, " offset "))) else { unparsed.push(id); continue };
        let site = after(l, " site=").unwrap_or_else(|| "none".into());
        let row = json!({"id": id, "tile": {"x": x, "y": y}, "offset": {"dx": dx, "dy": dy}, "tiles": dx + dy, "site": site});
        if dx + dy > DISPLACEMENT_THRESHOLD {
            displaced.push(row.clone());
        }
        if site == "outside" {
            outside_site.push(row.clone());
        }
        placements.push(row);
    }
    // Towns the engine generated itself (not from any source), as the GameScript renamed them.
    let engine: Vec<Value> = rw.iter().filter(|l| l.contains("rw: engine town ")).map(|l| {
        let (x, y) = pair(after(l, " at ")).unwrap_or((0, 0));
        json!({"id": after(l, "rw: engine town ").unwrap_or_default(), "name": between(l, "name=", " was=").unwrap_or_default(),
               "engine_name": between(l, " was=", " at ").unwrap_or_default(), "tile": {"x": x, "y": y},
               "population": after(l, " population ").and_then(|p| p.parse::<u64>().ok()), "evidence": "HYPOTHETICAL"})
    }).collect();
    // The reload: the counts the resumed script sees must be the counts the placing one left.
    let num = |l: &str, key: &str| after(l, key).and_then(|v| v.parse::<u64>().ok());
    let done_l = rw.iter().find(|l| l.contains("rw: done")).map(|l| l.to_string()).unwrap_or_default();
    let res_l = rw.iter().find(|l| l.contains("rw: resumed")).map(|l| l.to_string());
    let counts = |l: &str| json!({"towns": num(l, "game_towns "), "industries": num(l, "game_industries "), "signs": num(l, "game_signs ")});
    let reload_ok = res_l.as_ref().map(|r| counts(r) == counts(&done_l)).unwrap_or(false);
    let report = json!({
        "schema": "real-world-loadcheck/v1",
        "binary": o.openttd.display().to_string(),
        "binary_sha256": std::fs::read(&o.openttd).map(|b| crate::net::sha256_hex(&b)).unwrap_or_default(),
        "args": args,
        "env": {"HOME": home.display().to_string()},
        "baseset": baseset_note,
        "pid": pid,
        "exit_code": status,
        "killed_after_timeout": killed,
        "seconds": start.elapsed().as_secs(),
        "gamescript_finished": done,
        "done": done_line,
        "placed": {"towns": [ot, wt], "industries": [oi, wi], "signs": [os, ws]},
        "roles_unresolved": rw.iter().filter(|l| l.contains("UNRESOLVED")).map(|l| l.to_string()).collect::<Vec<_>>(),
        "failures": rw.iter().filter(|l| l.contains("FAILED")).map(|l| l.to_string()).collect::<Vec<_>>(),
        "labelled": {"towns_and_industries": [labelled, ot + oi], "unlabelled": unlabelled},
        "placements_unparsed": unparsed,
        "placements": placements,
        "displacement_threshold_tiles": DISPLACEMENT_THRESHOLD,
        "displaced_beyond_threshold": displaced,
        "relocated_outside_site": outside_site,
        "engine_generated": engine,
        "reload": {"loaded": sent_load, "resumed": res_l.is_some(), "after_placement": counts(&done_l),
                   "after_reload": res_l.as_ref().map(|r| counts(r)), "unchanged": reload_ok},
        "save": save.as_ref().map(|b| json!({"file": save_file.display().to_string(), "bytes": b.len(), "sha256": crate::net::sha256_hex(b)})),
        "rw_lines": rw,
        "log": work.join("openttd.log").display().to_string(),
    });
    std::fs::write(work.join("loadcheck.json"), crate::snapshot::to_bytes(&report)).map_err(|e| e.to_string())?;
    let code = if !done {
        7
    } else if ot == wt && oi == wi && os == ws && save.is_some() && reload_ok && unlabelled.is_empty()
        && unparsed.is_empty() && placements.len() == ot + oi {
        0
    } else {
        6
    };
    Ok((report, code))
}
