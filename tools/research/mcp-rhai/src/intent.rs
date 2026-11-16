//! RESEARCH-ONLY: a sentence from the owner ("najdi nejbližší les + pilu", "vybuduj nejlukrativnější
//! trať", "find the most productive coal mine") turned into a planner query over the live world.
//!
//! The nouns come from the game, not from a keyword list: every industry type name and cargo name in
//! every language file of the game (src/lang/*.txt, all case forms such as Czech .gen/.loc), joined
//! to the English name the world graph uses by its string id; town names come from openttd://towns.
//! The only hand vocabulary is the orderings (nearest, most productive, most profitable) and the two
//! actions (find, build), in Czech and English.
use std::collections::BTreeMap;

use serde_json::{json, Value};

/// Lower-case and strip diacritics, so "nejbližší" and "nejblizsi" match alike.
pub fn norm(s: &str) -> String {
    s.to_lowercase().chars().map(|c| match c {
        'á' | 'à' | 'â' | 'ä' | 'ã' | 'å' => 'a', 'č' | 'ç' | 'ć' => 'c', 'ď' => 'd',
        'é' | 'è' | 'ê' | 'ë' | 'ě' => 'e', 'í' | 'ì' | 'î' | 'ï' => 'i', 'ľ' | 'ĺ' | 'ł' => 'l',
        'ň' | 'ñ' | 'ń' => 'n', 'ó' | 'ò' | 'ô' | 'ö' | 'õ' | 'ő' => 'o', 'ř' | 'ŕ' => 'r',
        'š' | 'ś' | 'ș' => 's', 'ť' | 'ț' => 't', 'ú' | 'ù' | 'û' | 'ü' | 'ů' | 'ű' => 'u',
        'ý' | 'ÿ' => 'y', 'ž' | 'ź' | 'ż' => 'z', c if c.is_alphanumeric() => c, _ => ' ',
    }).collect()
}

fn words(s: &str) -> Vec<String> { norm(s).split_whitespace().map(str::to_string).collect() }

/// Two words are the same word in another case form when they share a stem: a common prefix of at
/// least 3 letters covering all but the last two letters of the shorter one.
fn same_word(a: &str, b: &str) -> bool {
    let (la, lb) = (a.chars().count(), b.chars().count());
    let shorter = la.min(lb);
    if shorter < 4 {
        // A short word ("les") only as the start of a longer case form ("lesem"), not as any prefix.
        let (s, l) = if la <= lb { (a, b) } else { (b, a) };
        return l.starts_with(s) && la.max(lb) <= shorter + 3;
    }
    let common = a.chars().zip(b.chars()).take_while(|(x, y)| x == y).count();
    common >= 3 && common + 2 >= shorter
}

/// The game's own vocabulary: (form words, English name, kind) for every industry type and cargo, in
/// every language file under `lang_dir`, with every case form.
pub fn vocabulary(lang_dir: &str) -> Vec<(Vec<String>, String, &'static str, String)> {
    let read = |file: &str| -> BTreeMap<String, Vec<String>> {
        let mut m: BTreeMap<String, Vec<String>> = BTreeMap::new();
        let Ok(text) = std::fs::read_to_string(format!("{lang_dir}/{file}")) else { return m };
        for line in text.lines() {
            let Some((key, value)) = line.split_once(':') else { continue };
            let key = key.trim();
            let id = key.split('.').next().unwrap_or("");
            if !(id.starts_with("STR_INDUSTRY_NAME_") || id.starts_with("STR_CARGO_PLURAL_")) { continue; }
            // Drop control codes ({G=f}, {BIG_FONT}) and leading prepositions of locative forms.
            let mut v = String::new();
            let mut depth = 0;
            for c in value.chars() {
                match c { '{' => depth += 1, '}' => depth -= 1, _ if depth == 0 => v.push(c), _ => {} }
            }
            m.entry(id.to_string()).or_default().push(v.trim().to_string());
        }
        m
    };
    let english = read("english.txt");
    let mut out = Vec::new();
    let files: Vec<String> = std::fs::read_dir(lang_dir).map(|d| d.filter_map(|e| e.ok())
        .map(|e| e.file_name().to_string_lossy().to_string()).filter(|n| n.ends_with(".txt")).collect()).unwrap_or_default();
    for f in files {
        for (id, forms) in read(&f) {
            let Some(en) = english.get(&id).and_then(|v| v.first()) else { continue };
            let kind = if id.starts_with("STR_INDUSTRY_NAME_") { "industry_type" } else { "cargo" };
            for form in forms {
                let mut w = words(&form);
                // "v uhelném dole", "na pile": a one- or two-letter preposition is not part of the name.
                if w.len() > 1 && w[0].chars().count() <= 2 { w.remove(0); }
                if !w.is_empty() { out.push((w, en.clone(), kind, f.trim_end_matches(".txt").to_string())); }
            }
        }
    }
    out.sort_by(|a, b| b.0.len().cmp(&a.0.len())); // longer names first: "coal mine" before "coal"
    out
}

const ORDERINGS: [(&str, &[&str]); 3] = [
    ("nearest", &["nejbliz", "nearest", "closest", "nejkratsi", "shortest"]),
    ("most_productive", &["nejproduktiv", "productive", "nejvetsi produk", "most produc", "largest"]),
    ("most_profitable", &["nejlukrativ", "nejvynosn", "nejzisk", "nejvyhodn", "profitable", "lucrative", "best", "nejlepsi"]),
];
const BUILD: [&str; 8] = ["vybuduj", "postav", "vystav", "build", "construct", "vybudovat", "postavit", "zaloz"];

/// Parse a sentence against the game's vocabulary and the live towns.
pub fn parse(text: &str, vocab: &[(Vec<String>, String, &'static str, String)], towns: &Value, present: &[String]) -> Value {
    let t = norm(text);
    let toks = words(text);
    let action = if BUILD.iter().any(|b| toks.iter().any(|w| w.starts_with(b))) { "build" } else { "find" };
    let ordering = ORDERINGS.iter().find(|(_, keys)| keys.iter().any(|k| t.contains(k))).map(|(o, _)| *o);
    // The sentence's language: the language file whose names match the most words (exactly, or as a
    // case form of 4+ letters)
    // (English on a tie). Names are then matched in that language and English only, and only names
    // that exist in this game (industry types on the map, cargos on offer).
    let mut score: BTreeMap<String, usize> = BTreeMap::new();
    for (form, en, _, lang) in vocab {
        if !present.iter().any(|p| p == en) { continue; }
        for w in form {
            // An exact word counts at any length ("les"); a case form only from 4 letters ("lesem").
            if toks.iter().any(|t| t == w || (w.chars().count() >= 4 && t.chars().count() >= 4 && same_word(t, w))) {
                *score.entry(lang.clone()).or_default() += 1;
            }
        }
    }
    let lang = score.iter().max_by(|a, b| a.1.cmp(b.1).then((b.0 == "english").cmp(&(a.0 == "english")))).map(|(l, _)| l.clone()).unwrap_or_else(|| "english".into());
    let mut used = vec![false; toks.len()];
    let mut subjects: Vec<(usize, Value)> = Vec::new();
    for (form, en, kind, l) in vocab {
        if (*l != lang && l != "english") || !present.iter().any(|p| p == en) { continue; }
        if form.len() > toks.len() { continue; }
        for start in 0..=toks.len() - form.len() {
            if (0..form.len()).any(|i| used[start + i]) { continue; }
            if (0..form.len()).all(|i| same_word(&toks[start + i], &form[i])) {
                if !subjects.iter().any(|(_, s)| s["name"] == en.as_str() && s["kind"] == *kind) {
                    subjects.push((start, json!({"kind": kind, "name": en, "matched": toks[start..start + form.len()].join(" ")})));
                }
                for i in 0..form.len() { used[start + i] = true; }
            }
        }
    }
    for town in towns.pointer("/towns").and_then(Value::as_array).into_iter().flatten() {
        let name = town["name"].as_str().unwrap_or("");
        let nw = words(name);
        if nw.is_empty() || nw.len() > toks.len() { continue; }
        for start in 0..=toks.len() - nw.len() {
            if (0..nw.len()).all(|i| !used[start + i] && same_word(&toks[start + i], &nw[i])) {
                subjects.push((start, json!({"kind": "town", "name": name, "id": town["id"], "x": town["x"], "y": town["y"]})));
                for i in 0..nw.len() { used[start + i] = true; }
            }
        }
    }
    subjects.sort_by_key(|(i, _)| *i);
    json!({"text": text, "language": lang, "action": action, "ordering": ordering,
           "subjects": subjects.into_iter().map(|(_, s)| s).collect::<Vec<_>>()})
}

fn num(v: &Value, k: &str) -> f64 { v.get(k).and_then(Value::as_f64).unwrap_or(0.0) }

fn production(ind: &Value) -> f64 {
    ind.get("produces").and_then(Value::as_array).into_iter().flatten().map(|p| num(p, "last_month")).sum()
}

fn dist(a: &Value, b: &Value) -> f64 { (num(a, "x") - num(b, "x")).abs() + (num(a, "y") - num(b, "y")).abs() }

/// Answer a parsed intent over the world graph and the plan (the output of mcp-plan): candidates in
/// the asked order, and for "build" the bot settings that would carry out the first one.
pub fn answer(intent: &Value, world: &Value, plan: &Value) -> Value {
    let industries: Vec<&Value> = world.get("nodes").and_then(Value::as_array).map(|n| n.iter().filter(|x| x["kind"] == "industry").collect()).unwrap_or_default();
    let types: Vec<String> = intent["subjects"].as_array().into_iter().flatten().filter(|s| s["kind"] == "industry_type").filter_map(|s| s["name"].as_str().map(str::to_string)).collect();
    let cargos: Vec<String> = intent["subjects"].as_array().into_iter().flatten().filter(|s| s["kind"] == "cargo").filter_map(|s| s["name"].as_str().map(str::to_string)).collect();
    let ordering = intent["ordering"].as_str().unwrap_or(if types.len() >= 2 { "nearest" } else { "most_profitable" });
    let of = |t: &str| -> Vec<&Value> { industries.iter().copied().filter(|i| i["name"] == t).collect() };
    let accepts = |ind: &Value, cargo: &str| ind.get("accepts").and_then(Value::as_array).into_iter().flatten().any(|a| a["name"] == cargo);
    let mut candidates: Vec<Value> = Vec::new();
    let how: String;
    if types.len() >= 2 {
        // A pair: producer of type 1, acceptor of type 2 that takes one of its cargos.
        how = format!("pairs {} -> {} ordered {}", types[0], types[1], ordering);
        for a in of(&types[0]) {
            let made: Vec<String> = a.get("produces").and_then(Value::as_array).into_iter().flatten().filter_map(|p| p["name"].as_str().map(str::to_string)).collect();
            for b in of(&types[1]) {
                let Some(cargo) = made.iter().find(|c| accepts(b, c)) else { continue };
                candidates.push(json!({"from": {"id": a["id"], "name": a["name"], "x": a["x"], "y": a["y"], "production": production(a)},
                    "to": {"id": b["id"], "name": b["name"], "x": b["x"], "y": b["y"]}, "cargo": cargo, "distance": dist(a, b)}));
            }
        }
        match ordering {
            "most_productive" => candidates.sort_by(|x, y| num(&y["from"], "production").total_cmp(&num(&x["from"], "production")).then(num(x, "distance").total_cmp(&num(y, "distance")))),
            "most_profitable" => {
                // Priced by the planner's freight pairs when it has the same pair.
                for c in candidates.iter_mut() {
                    let score = plan["freight"].as_array().into_iter().flatten().find(|f| f["producer"]["id"] == c["from"]["id"] && f["acceptor"]["id"] == c["to"]["id"]).map(|f| num(f, "score"));
                    c["score"] = json!(score);
                }
                candidates.sort_by(|x, y| num(y, "score").total_cmp(&num(x, "score")));
            }
            _ => candidates.sort_by(|x, y| num(x, "distance").total_cmp(&num(y, "distance"))),
        }
    } else if types.len() == 1 {
        how = format!("industries of type {} ordered {}", types[0], ordering);
        for a in of(&types[0]) {
            candidates.push(json!({"id": a["id"], "name": a["name"], "x": a["x"], "y": a["y"], "production": production(a), "produces": a["produces"]}));
        }
        candidates.sort_by(|x, y| num(y, "production").total_cmp(&num(x, "production")));
    } else {
        // No place named: the planner's candidates, freight and passengers, best score first, filtered
        // by a named cargo when there is one.
        how = "the planner's candidates ordered by score".into();
        let mut all: Vec<Value> = plan["freight"].as_array().cloned().unwrap_or_default();
        for (_, ps) in plan["by_mode"].as_object().cloned().unwrap_or_default() { all.extend(ps.as_array().cloned().unwrap_or_default()); }
        if !cargos.is_empty() { all.retain(|c| c["cargo"].as_str().is_some_and(|x| cargos.iter().any(|k| k == x)) || (cargos.iter().any(|k| k == "Passengers") && c["kind"] != "freight")); }
        all.sort_by(|x, y| num(y, "score").total_cmp(&num(x, "score")));
        candidates = all;
    }
    candidates.truncate(10);
    let mut out = json!({"how": how, "ordering": ordering, "candidates": candidates, "solver": plan["portfolio"]["solver"], "kernel": plan["portfolio"]["kernel"]});
    if intent["action"] == "build" {
        let first = out["candidates"].get(0).cloned().unwrap_or(Value::Null);
        let num_id = |v: &Value| v.as_str().and_then(|s| s.rsplit(':').next()).and_then(|n| n.parse::<i64>().ok()).or_else(|| v.as_i64());
        let settings = if first.get("from").is_some() {
            let cargo_id = plan["freight"].as_array().into_iter().flatten().find(|f| f["cargo"] == first["cargo"]).map(|f| f["cargo_id"].clone());
            json!({"fr_from": num_id(&first["from"]["id"]), "fr_to": num_id(&first["to"]["id"]), "fr_cargo": cargo_id})
        } else if first.get("producer").is_some() {
            json!({"fr_from": num_id(&first["producer"]["id"]), "fr_to": num_id(&first["acceptor"]["id"]), "fr_cargo": first["cargo_id"]})
        } else if first.get("a").is_some() {
            let m = first["mode"].as_str().unwrap_or("rail");
            json!({format!("{m}_a"): first["a"]["id"], format!("{m}_b"): first["b"]["id"]})
        } else { Value::Null };
        out["build"] = json!({"settings": settings, "execution": "the bot builds it once these settings reach it: openttd.ai.set_setting (approval-gated) or the next start_ai"});
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn czech_cases_match_the_same_word() {
        assert!(same_word("pilu", "pila"));
        assert!(same_word("lesem", "les"));
        assert!(same_word("nejblizsi", "nejblizsi"));
        assert!(!same_word("les", "lesk_long_other"));
        assert!(!same_word("a", "voda"));
    }
    #[test]
    fn diacritics_fold() { assert_eq!(norm("Nejbližší Pilu!"), "nejblizsi pilu "); }
}
