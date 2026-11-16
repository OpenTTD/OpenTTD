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

/// Character trigram counts of each language file's strings (control codes dropped, lower case,
/// diacritics kept, each word padded with spaces): the game's own texts as a language identifier.
/// It breaks a tie between languages whose names match the same words ("les" is Czech, Slovak and
/// Slovenian), from how the rest of the sentence is spelled ("nej-" is Czech, "naj-" Slovak).
pub type Profiles = BTreeMap<String, (std::collections::HashMap<String, u32>, u32)>;

fn trigrams(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    for w in text.to_lowercase().split(|c: char| !c.is_alphanumeric()).filter(|w| !w.is_empty()) {
        let padded: Vec<char> = format!(" {w} ").chars().collect();
        for g in padded.windows(3) { out.push(g.iter().collect()); }
    }
    out
}

pub fn profiles(lang_dir: &str) -> Profiles {
    let mut out = Profiles::new();
    let Ok(dir) = std::fs::read_dir(lang_dir) else { return out };
    for e in dir.filter_map(|e| e.ok()) {
        let name = e.file_name().to_string_lossy().to_string();
        let Some(lang) = name.strip_suffix(".txt") else { continue };
        let Ok(text) = std::fs::read_to_string(e.path()) else { continue };
        // A language file starts with its ##name header (CMakeLists.txt lives there too).
        if !text.starts_with("##name") { continue; }
        let mut counts = std::collections::HashMap::new();
        let mut total = 0u32;
        for line in text.lines() {
            if line.starts_with('#') { continue; }
            let Some((_, value)) = line.split_once(':') else { continue };
            let mut v = String::new();
            let mut depth = 0;
            for c in value.chars() {
                match c { '{' => depth += 1, '}' => depth -= 1, _ if depth == 0 => v.push(c), _ => {} }
            }
            for g in trigrams(&v) { *counts.entry(g).or_insert(0) += 1; total += 1; }
        }
        if total > 0 { out.insert(lang.to_string(), (counts, total)); }
    }
    out
}

/// The log-likelihood of `text` under a language's trigram profile, smoothed toward the trigram's
/// frequency over every language (Dirichlet, weight 1000). Add-one smoothing favoured the smallest
/// files, whose few trigrams leave much probability to the unseen; this way a language wins only by
/// trigrams that are typical of it ("nej", "ějš" for Czech).
fn likelihood(text: &str, profile: &(std::collections::HashMap<String, u32>, u32), all: &Profiles) -> f64 {
    let (counts, total) = profile;
    let grand: f64 = all.values().map(|(_, n)| *n as f64).sum::<f64>().max(1.0);
    trigrams(text).iter().map(|g| {
        let bg = (all.values().map(|(c, _)| *c.get(g).unwrap_or(&0) as f64).sum::<f64>() + 0.5) / grand;
        ((*counts.get(g).unwrap_or(&0) as f64 + 1000.0 * bg) / (*total as f64 + 1000.0)).ln()
    }).sum()
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
#[allow(dead_code)]
pub fn parse(text: &str, vocab: &[(Vec<String>, String, &'static str, String)], towns: &Value, present: &[String]) -> Value {
    parse_with(text, vocab, towns, present, None)
}

/// `parse`, with the languages' trigram profiles (`profiles`) to break a tie between languages that
/// match the same names.
pub fn parse_with(text: &str, vocab: &[(Vec<String>, String, &'static str, String)], towns: &Value, present: &[String],
                  profiles: Option<&Profiles>) -> Value {
    let t = norm(text);
    let toks = words(text);
    let action = if BUILD.iter().any(|b| toks.iter().any(|w| w.starts_with(b))) { "build" } else { "find" };
    let ordering = ORDERINGS.iter().find(|(_, keys)| keys.iter().any(|k| t.contains(k))).map(|(o, _)| *o);
    // A word that says what to do or how to order is not also a name: "postav" (build) would
    // otherwise match "pošta" (mail) as a case form.
    let command_word: Vec<bool> = toks.iter().map(|w| BUILD.iter().any(|b| w.starts_with(b))
        || ORDERINGS.iter().any(|(_, keys)| keys.iter().any(|k| !k.contains(' ') && w.starts_with(k)))).collect();
    // The sentence's language: the language file whose names match the most words (exactly, or as a
    // case form of 4+ letters)
    // (English on a tie). Names are then matched in that language and English only, and only names
    // that exist in this game (industry types on the map, cargos on offer).
    // Names that exist in this game decide first, so a short English word ("a", "the") that is some
    // other language's industry name does not switch the language. When no such name matches (a
    // game whose industries differ, or none published yet), every name decides: the language is a
    // property of the sentence, and "najdi nejbližší les + pilu" stays Czech on any game.
    // Each language scores the distinct words of the sentence its names match, so a language whose
    // names repeat a word over many case forms does not outvote one that matches more of the text.
    let score_by = |only_present: bool| -> BTreeMap<String, std::collections::BTreeSet<usize>> {
        let mut score: BTreeMap<String, std::collections::BTreeSet<usize>> = BTreeMap::new();
        for (form, en, _, lang) in vocab {
            if only_present && !present.iter().any(|p| p == en) { continue; }
            for w in form {
                // An exact word counts at any length ("les"); a case form only from 4 letters ("lesem").
                for (i, t) in toks.iter().enumerate() {
                    if command_word[i] { continue; }
                    if t == w || (w.chars().count() >= 4 && t.chars().count() >= 4 && same_word(t, w)) {
                        score.entry(lang.clone()).or_default().insert(i);
                    }
                }
            }
        }
        score
    };
    let (mut score, mut language_from) = (score_by(true), "names in this game");
    // No name of this game matched: with the profiles the sentence's spelling decides below (names
    // of absent industries in 60 languages match short words by chance); without them, every name.
    if score.is_empty() && profiles.is_none() {
        score = score_by(false);
        language_from = "every name of every language (none in this game matched)";
    }
    // A tie between languages matching as many words is broken by how the whole sentence is spelled
    // (trigram profiles of the game's language files), then English wins (english_AU and english_US
    // share its words and spelling).
    let best = score.values().map(|s| s.len()).max().unwrap_or(0);
    let tied: Vec<&String> = score.iter().filter(|(_, s)| s.len() == best).map(|(l, _)| l).collect();
    let spelled = |l: &String| profiles.and_then(|all| all.get(l).map(|p| likelihood(text, p, all)));
    let mut lang = tied.iter().copied().max_by(|a, b| {
        let (sa, sb) = (spelled(a), spelled(b));
        let by_spelling = match (sa, sb) { (Some(x), Some(y)) => x.total_cmp(&y), _ => std::cmp::Ordering::Equal };
        // English variants spell alike: compare English last so "english" wins among them.
        let english = |l: &str| l.starts_with("english");
        if english(a) && english(b) { return (*a == "english").cmp(&(*b == "english")); }
        by_spelling.then((*a == "english").cmp(&(*b == "english")))
    }).cloned().unwrap_or_else(|| "english".into());
    if tied.len() > 1 && profiles.is_some() { language_from = if language_from == "names in this game" { "names in this game, the tie by spelling" } else { "every name, the tie by spelling" }; }
    if score.is_empty() {
        // No name at all: the spelling alone decides, when the profiles are at hand.
        if let Some(p) = profiles {
            if let Some((l, _)) = p.iter().map(|(l, pr)| (l, likelihood(text, pr, p))).max_by(|a, b| a.1.total_cmp(&b.1)) {
                lang = l.clone();
            }
            // English variants share the words and spelling of English.
            if lang.starts_with("english") { lang = "english".into(); }
            language_from = "no name matched: the sentence's spelling";
        } else {
            language_from = "no name matched: English by default";
        }
    }
    // Why: the three best languages with the words each matched.
    let mut ranked: Vec<(&String, &std::collections::BTreeSet<usize>)> = score.iter().collect();
    ranked.sort_by(|a, b| b.1.len().cmp(&a.1.len()));
    let language_scores: Vec<Value> = ranked.into_iter().take(3)
        .map(|(l, ix)| json!({"language": l, "words": ix.iter().map(|&i| toks[i].clone()).collect::<Vec<_>>()})).collect();
    let mut used = command_word.clone();
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
    json!({"text": text, "language": lang, "language_from": language_from, "language_scores": language_scores,
           "action": action, "ordering": ordering,
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
            // The cargo's id from the producer's own produces entry in the world graph (where the
            // planner's freight pairs take it too); the planner's list may hold no pair of that
            // cargo, which left fr_cargo null for "vystav trať mezi nejproduktivnějším lesem a pilou".
            let cargo_id = industries.iter().find(|i| i["id"] == first["from"]["id"])
                .and_then(|i| i["produces"].as_array()?.iter().find(|p| p["name"] == first["cargo"]).map(|p| p["cargo"].clone()))
                .filter(|v| !v.is_null())
                .or_else(|| plan["freight"].as_array().into_iter().flatten().find(|f| f["cargo"] == first["cargo"]).map(|f| f["cargo_id"].clone()));
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

    fn vocab() -> Vec<(Vec<String>, String, &'static str, String)> {
        let v = |w: &str, en: &str, lang: &str| (words(w), en.to_string(), "industry_type", lang.to_string());
        vec![v("les", "Forest", "czech"), v("pila", "Sawmill", "czech"), v("forest", "Forest", "english"),
             v("sawmill", "Sawmill", "english"), v("a", "Water Tower", "slovak")]
    }

    #[test]
    fn a_czech_sentence_stays_czech_on_a_game_without_those_industries() {
        let towns = json!({"towns": []});
        let on_none = parse("najdi nejbližší les + pilu", &vocab(), &towns, &[]);
        assert_eq!(on_none["language"], "czech");
        assert_eq!(on_none["subjects"], json!([]));
        assert!(on_none["language_from"].as_str().unwrap().starts_with("every name"));
        let present = ["Forest".to_string(), "Sawmill".to_string()];
        let on_temperate = parse("najdi nejbližší les + pilu", &vocab(), &towns, &present);
        assert_eq!(on_temperate["language"], "czech");
        assert_eq!(on_temperate["subjects"].as_array().unwrap().len(), 2);
        assert_eq!(on_temperate["language_from"], "names in this game");
    }

    #[test]
    fn a_short_english_word_does_not_switch_the_language_when_present_names_match() {
        let present = ["Forest".to_string()];
        let p = parse("find a forest", &vocab(), &json!({"towns": []}), &present);
        assert_eq!(p["language"], "english");
    }

    #[test]
    fn a_built_pair_carries_its_cargo_id_from_the_world_graph_when_the_planner_has_no_such_pair() {
        let world = json!({"nodes": [
            {"kind": "industry", "id": "industry:3", "name": "Forest", "x": 1, "y": 1, "produces": [{"name": "Wood", "cargo": 7, "last_month": 80}]},
            {"kind": "industry", "id": "industry:18", "name": "Sawmill", "x": 20, "y": 1, "accepts": [{"name": "Wood"}]}]});
        let intent = json!({"action": "build", "ordering": "most_productive",
            "subjects": [{"kind": "industry_type", "name": "Forest"}, {"kind": "industry_type", "name": "Sawmill"}]});
        let a = answer(&intent, &world, &json!({"freight": []}));
        assert_eq!(a["build"]["settings"], json!({"fr_from": 3, "fr_to": 18, "fr_cargo": 7}));
    }

    #[test]
    fn a_command_word_is_not_also_a_name() {
        let v = |w: &str, en: &str, lang: &str| (words(w), en.to_string(), "cargo", lang.to_string());
        let vocab = vec![v("pošta", "Mail", "czech"), v("les", "Forest", "czech")];
        let present = ["Mail".to_string(), "Forest".to_string()];
        let p = parse("postav nejvýnosnější linku", &vocab, &json!({"towns": []}), &present);
        assert_eq!(p["action"], "build");
        assert_eq!(p["subjects"], json!([]));
        // The other words still name things.
        let q = parse("postav trať, les", &vocab, &json!({"towns": []}), &present);
        assert_eq!(q["subjects"][0]["name"], "Forest");
    }

    fn profile(text: &str) -> (std::collections::HashMap<String, u32>, u32) {
        let mut c = std::collections::HashMap::new();
        let mut n = 0;
        for g in trigrams(text) { *c.entry(g).or_insert(0) += 1; n += 1; }
        (c, n)
    }

    #[test]
    fn the_spelling_breaks_a_tie_and_decides_when_no_name_of_this_game_matched() {
        let mut p = Profiles::new();
        p.insert("czech".into(), profile("nejlepší nejbližší nejvyšší vybuduj trať lesem pila najít"));
        p.insert("slovak".into(), profile("najlepší najbližší najvyšší postav trať lesom píla nájsť"));
        p.insert("english".into(), profile("the best nearest highest build line forest sawmill find"));
        let v = |w: &str, en: &str, lang: &str| (words(w), en.to_string(), "industry_type", lang.to_string());
        let vocab = vec![v("les", "Forest", "czech"), v("les", "Forest", "slovak"), v("forest", "Forest", "english")];
        let towns = json!({"towns": []});
        let tie = parse_with("najdi nejbližší les", &vocab, &towns, &["Forest".to_string()], Some(&p));
        assert_eq!(tie["language"], "czech");
        assert_eq!(tie["subjects"][0]["name"], "Forest");
        let none = parse_with("vybuduj nejlukrativnější trať", &vocab, &towns, &[], Some(&p));
        assert_eq!(none["language"], "czech");
        let english = parse_with("build the most lucrative line", &vocab, &towns, &[], Some(&p));
        assert_eq!(english["language"], "english");
    }

    #[test]
    fn english_wins_a_tie_with_its_variants_and_many_case_forms_do_not_outvote() {
        let v = |w: &str, en: &str, lang: &str| (words(w), en.to_string(), "industry_type", lang.to_string());
        // english_AU sorts after english and shares its words; polish repeats "kopalni" in many forms.
        let vocab = vec![v("coal mine", "Coal Mine", "english"), v("coal mine", "Coal Mine", "english_AU"),
                         v("kopalni", "Coal Mine", "polish"), v("kopalnia", "Coal Mine", "polish"), v("kopalniach", "Coal Mine", "polish")];
        let present = ["Coal Mine".to_string()];
        let p = parse("find the nearest coal mine", &vocab, &json!({"towns": []}), &present);
        assert_eq!(p["language"], "english");
        assert_eq!(p["subjects"][0]["name"], "Coal Mine");
    }
}
