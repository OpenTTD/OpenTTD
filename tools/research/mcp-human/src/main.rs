//! RESEARCH-ONLY: play OpenTTD the way a person does, through a GUI client's own MCP server.
//!
//! The hand moves the client's visible cursor with human motion (Fitts' law timing,
//! minimum-jerk curved paths, overshoot and correction, tremor), finds buttons by what they say
//! on screen (openttd.ui.windows), clicks them, and drags roads across tiles with the game's own
//! tools; the camera glides to wherever it works. Everything goes through the client's ordinary
//! input path, so what the owner sees is exactly what a person at that mouse would produce, and
//! the commands reach the server through the network like any player's.
//!
//! The client must run with PRISMATIC_MCP_INPUT=1 (the owner's switch), and the hand yields
//! whenever the owner moves the real mouse.

mod actions;
mod algoglyph;
mod economy;
mod fleet;
mod hand;
mod mcp;
mod motion;
mod play;
mod plan;
mod rail;
mod requests;

use hand::{Hand, Target};
use mcp::Mcp;
use motion::Model;
use serde_json::json;

const USAGE: &str = "usage: mcp-human --token <client-token.json> [--seed N] [--speed F] <command>

commands:
  golden                     print the motion model's golden vectors (no token needed)
  check                      the client offers the UI and input tools, and input is armed
  state                      cursor, buttons, active tool and tile selection (JSON)
  windows                    every window on screen with its widgets (JSON)
  find <text>                the front-most widget whose tooltip or text contains <text>
  tile <x> <y>               where a map tile is on screen, scrolling to it if needed (JSON)
  move <x> <y>               move the cursor there with human motion
  click <x> <y>              move there and click
  click-widget <text>        find a widget by what it says and click it
  look                       look around the interface like a person sitting down
  road <x1> <y1> <x2> <y2>   plan and drag a road between two tiles with the autoroad tool
  connect [<town> <town>]    connect two towns by road (default: the largest and a neighbour)
  serve <town> <town>        a bus service: road if needed, two stops, a depot, a bus with orders
  play [<rounds>]            look around, then serve town pairs round after round
  rail [<max> [<min>]]       a rail line within [min, max] tiles: both stations at the shared planner's sites (MCP_PLAN),
                             AlgoGlyph's track plan between them, laid with the rail tools, checked
  yard [<pairs>]             a transfer yard the planner proved (mcp-plan --yards): line A, then line B
                             from a platform beside A's, B's track planned again with A built
  execute <actions.json|->   plan the actions with AlgoGlyph's action-plan kernel (DAG, cash per
                             step, refused when it would overdraw), show it, then do each step
  renew <vehicle>             send the vehicle to its depot, buy a copy there (its orders copied),
                             start the copy, sell the old one: all through the windows
  sell <vehicle>             send the vehicle to its depot and sell it there
  fleet                      the company's vehicles that are due (old, or losing money), as JSON
  game [<minutes>] [--borrow-below N] [--repay-above N] [--rail-above N] [--renew-age DAYS]
                             run the company: expand, add buses where they earn, borrow (asking
                             the owner first) while money is under N, repay part of the loan
                             while it is over N, renew vehicles at DAYS (default: when the game
                             calls them old) and sell ones losing money for two years, watch";

fn arg_num<T: std::str::FromStr>(args: &[String], i: usize) -> Result<T, String> {
    args.get(i).ok_or_else(|| USAGE.to_string())?.parse::<T>().map_err(|_| format!("not a number: {}", args[i]))
}

fn run() -> Result<(), String> {
    let mut args: Vec<String> = std::env::args().skip(1).collect();
    let mut token = std::env::var("MCP_HUMAN_TOKEN").unwrap_or_default();
    let mut seed: u64 = 1;
    let mut model = Model::default();
    while let Some(first) = args.first().cloned() {
        match first.as_str() {
            "--token" => { token = args.get(1).cloned().ok_or(USAGE)?; args.drain(0..2); }
            "--seed" => { seed = arg_num(&args, 1)?; args.drain(0..2); }
            "--speed" => { model.speed = arg_num(&args, 1)?; args.drain(0..2); }
            "-h" | "--help" => { println!("{USAGE}"); return Ok(()); }
            _ => break,
        }
    }
    if args.first().map(String::as_str) == Some("golden") {
        // The motion model's golden vectors (no game needed): the parity reference for AlgoGlyph.
        println!("{}", serde_json::to_string_pretty(&motion::golden()).unwrap_or_default());
        return Ok(());
    }
    if token.is_empty() || args.is_empty() {
        return Err(USAGE.to_string());
    }
    let mut hand = Hand::new(Mcp::new(&token), model, seed);
    match args[0].as_str() {
        "check" => {
            hand.require_tools()?;
            println!("ok: UI and input tools present, input armed");
        }
        "state" => println!("{}", serde_json::to_string_pretty(&hand.state()?).unwrap_or_default()),
        "windows" => println!("{}", serde_json::to_string_pretty(&hand.windows()?).unwrap_or_default()),
        "find" => match hand.find_widget(args.get(1).ok_or(USAGE)?)? {
            Some((w, wd)) => println!("{}", json!({"window": {"class": w["class"], "caption": w["caption"]}, "widget": wd})),
            None => return Err("nothing on screen says that".into()),
        },
        "tile" => {
            let t = hand.tile(arg_num(&args, 1)?, arg_num(&args, 2)?)?;
            println!("{}", json!({"x": t.x, "y": t.y, "w": t.w, "h": t.h}));
        }
        "move" => hand.move_to(Target { x: arg_num(&args, 1)?, y: arg_num(&args, 2)?, w: 20, h: 20 })?,
        "click" => hand.click(Target { x: arg_num(&args, 1)?, y: arg_num(&args, 2)?, w: 20, h: 20 })?,
        "click-widget" => {
            hand.click_widget(args.get(1).ok_or(USAGE)?)?;
        }
        "look" => hand.look_around()?,
        "road" => {
            let n = hand.build_road((arg_num(&args, 1)?, arg_num(&args, 2)?), (arg_num(&args, 3)?, arg_num(&args, 4)?))?;
            println!("ok: {n} drags");
        }
        "connect" => {
            let pair = if args.len() >= 3 { Some((arg_num(&args, 1)?, arg_num(&args, 2)?)) } else { None };
            let (a, b) = hand.connect_towns(pair)?;
            println!("ok: {a} and {b} connected");
        }
        "serve" => {
            let (ta, tb): (u64, u64) = (arg_num(&args, 1)?, arg_num(&args, 2)?);
            let towns = hand.mcp.read("openttd://towns")?;
            let list = towns.get("towns").and_then(serde_json::Value::as_array).cloned().unwrap_or_default();
            let find = |id: u64| list.iter().find(|t| t.get("id").and_then(serde_json::Value::as_u64) == Some(id)).cloned().ok_or("no such town");
            let stops = hand.serve_towns(&find(ta)?, &find(tb)?)?;
            println!("ok: stops {stops:?}");
        }
        "rail" => {
            // A rail line: the shortest planner candidate (within the distance) that builds.
            let max: f64 = if args.len() > 1 { arg_num(&args, 1)? } else { 60.0 };
            // An optional minimum distance builds a further line once the nearer ones exist.
            let min: f64 = if args.len() > 2 { arg_num(&args, 2)? } else { 0.0 };
            hand.require_tools()?;
            let candidates: Vec<serde_json::Value> = hand.rail_candidates(max)?.into_iter()
                .filter(|c| c["distance"].as_f64().unwrap_or(0.0) >= min).collect();
            hand.say(&format!("{} rail candidate(s) within {max} tiles", candidates.len()));
            let s = hand.first_rail_service(&candidates.into_iter().take(4).collect::<Vec<_>>())?;
            println!("ok: rail line {}: stations at {},{} and {},{}, {} pieces of track", s.label, s.line.from.x, s.line.from.y, s.line.to.x, s.line.to.y, s.line.pieces.len());
            println!("ok: train {} ({}) runs between the stations, depot at {},{}", s.train, s.consist, s.depot.0, s.depot.1);
        }
        "yard" => {
            // A transfer yard (src/yards.rs in mcp-rhai proves it): line A, then line B from a
            // platform beside A's, B re-planned on the map with A built.
            let pairs: usize = if args.len() > 1 { arg_num(&args, 1)? } else { 4 };
            hand.require_tools()?;
            let yards = hand.proven_yards(pairs)?;
            hand.say(&format!("{} proven yard(s) on this map", yards.len()));
            let first = yards.first().ok_or("no proven transfer yard on this map (see GET /api/plan?track=6&yards=4)")?;
            let (a, b) = hand.yard_service(first)?;
            println!("ok: yard line A {}: stations at {},{} and {},{}, train {}", a.label, a.line.from.x, a.line.from.y, a.line.to.x, a.line.to.y, a.train);
            println!("ok: yard line B {}: stations at {},{} and {},{}, train {}", b.label, b.line.from.x, b.line.from.y, b.line.to.x, b.line.to.y, b.train);
        }
        "execute" => {
            // A plan first, then its execution: the actions go to AlgoGlyph's action-plan kernel.
            let src = args.get(1).ok_or("execute <actions.json | ->")?;
            let text = if src == "-" { std::io::read_to_string(std::io::stdin()).map_err(|e| e.to_string())? } else { std::fs::read_to_string(src).map_err(|e| format!("{src}: {e}"))? };
            let actions: serde_json::Value = serde_json::from_str(&text).map_err(|e| format!("{src}: {e}"))?;
            hand.require_tools()?;
            hand.execute(actions.get("actions").unwrap_or(&actions))?;
            println!("ok: the plan was executed");
        }
        "renew" | "sell" => {
            let id: u64 = arg_num(&args, 1)?;
            hand.require_tools()?;
            let fate = if args[0] == "renew" { fleet::Fate::Renew } else { fleet::Fate::Sell };
            let copy = hand.retire(id, fate)?;
            println!("{}", fleet::report(id, fate, copy));
            match copy {
                Some(c) => println!("ok: vehicle {id} renewed by its copy {c}"),
                None => println!("ok: vehicle {id} sold"),
            }
        }
        "fleet" => {
            let due = hand.due_vehicles(fleet::Rules::default())?;
            let list: Vec<_> = due.iter().map(|(id, f, why)| json!({"vehicle": id, "fate": format!("{f:?}").to_lowercase(), "why": why})).collect();
            println!("{}", serde_json::Value::Array(list));
        }
        "game" => {
            // The minutes are optional, so `game --borrow-below 5` plays the default 20.
            let minutes: u64 = if args.len() > 1 && !args[1].starts_with("--") { arg_num(&args, 1)? } else { 20 };
            let mut policy = economy::Policy::default();
            // --borrow-below N: borrow (asking the owner first) whenever money is under N.
            if let Some(i) = args.iter().position(|a| a == "--borrow-below") {
                policy.borrow_below = arg_num(&args, i + 1)?;
            }
            // --rail-above N: start a rail line (planner sites, AlgoGlyph track, a train) once money is over N.
            if let Some(i) = args.iter().position(|a| a == "--rail-above") {
                policy.rail_above = arg_num(&args, i + 1)?;
            }
            // --repay-above N: pay part of the loan back whenever money is over N.
            if let Some(i) = args.iter().position(|a| a == "--repay-above") {
                policy.repay_above = arg_num(&args, i + 1)?;
            }
            // --renew-age DAYS: renew vehicles from this age on, not only when the game calls them old.
            if let Some(i) = args.iter().position(|a| a == "--renew-age") {
                policy.fleet.renew_age = Some(arg_num(&args, i + 1)?);
            }
            let services = hand.play_game(minutes, policy)?;
            for s in &services {
                println!("service {}: {} bus(es), stops {:?}, depot {:?}", s.name, s.buses, s.stops, s.depot);
            }
            println!("ok: {} service(s), {} bus(es)", services.len(), services.iter().map(|s| s.buses).sum::<usize>());
        }
        "play" => {
            let rounds: usize = if args.len() > 1 { arg_num(&args, 1)? } else { 3 };
            hand.require_tools()?;
            hand.look_around()?;
            // The owner decides; no answer in time is not a no, and the default is to play.
            if hand.ask_owner("May I build bus services for your company now?", 8)? == Some(false) {
                println!("ok: the owner said no");
                return Ok(());
            }
            let pairs = hand.town_pairs()?;
            let mut served = 0;
            for (a, b) in pairs {
                if served >= rounds {
                    break;
                }
                let n = |t: &serde_json::Value| t.get("name").and_then(serde_json::Value::as_str).unwrap_or("?").to_string();
                hand.say(&format!("round {}/{rounds}: {} and {}", served + 1, n(&a), n(&b)));
                match hand.serve_towns(&a, &b) {
                    Ok(_) => {
                        served += 1;
                        hand.say(&format!("a bus runs between {} and {}", n(&a), n(&b)));
                    }
                    Err(e) if economy::game_closed(&e) => {
                        hand.say("the game was closed: stop playing");
                        break;
                    }
                    Err(e) => {
                        hand.say(&format!("skip {} and {}: {e}", n(&a), n(&b)));
                        // Leave a clean screen for the next pair, whatever failed.
                        let _ = hand.escape();
                        let _ = hand.tidy();
                    }
                }
                hand.think(800.0, 1600.0);
            }
            if served > 0 {
                hand.watch(8)?;
            }
            println!("ok: {served} town pair(s) served");
        }
        other => return Err(format!("unknown command '{other}'\n{USAGE}")),
    }
    Ok(())
}

fn main() {
    if let Err(e) = run() {
        eprintln!("mcp-human: {e}");
        // A wrong command line is a usage error (2), like every other research tool; anything else is 1.
        let usage = e.contains("usage: mcp-human") || e.starts_with("not a number");
        std::process::exit(if usage { 2 } else { 1 });
    }
}
