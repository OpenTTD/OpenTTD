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

mod hand;
mod mcp;
mod motion;
mod play;
mod plan;

use hand::{Hand, Target};
use mcp::Mcp;
use motion::Model;
use serde_json::json;

const USAGE: &str = "usage: mcp-human --token <client-token.json> [--seed N] [--speed F] <command>

commands:
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
  play [<rounds>]            look around, then serve town pairs round after round";

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
        "play" => {
            let rounds: usize = if args.len() > 1 { arg_num(&args, 1)? } else { 3 };
            hand.require_tools()?;
            hand.look_around()?;
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
                    Err(e) => {
                        hand.say(&format!("skip {} and {}: {e}", n(&a), n(&b)));
                        // Leave a clean screen for the next pair, whatever failed.
                        let _ = hand.escape();
                        let _ = hand.tidy();
                    }
                }
                hand.think(800.0, 1600.0);
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
        std::process::exit(1);
    }
}
