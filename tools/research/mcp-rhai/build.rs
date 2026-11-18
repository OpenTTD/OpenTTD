//! RESEARCH-ONLY. Where the linker finds the system libduckdb for the `store` feature (ADR 0033).
//!
//! The duckdb crate is used without its bundled C++ build, and libduckdb-sys then asks only for
//! `-lduckdb`. A `.cargo/config.toml` setting DUCKDB_LIB_DIR is read only when cargo runs inside this
//! directory, and mcp-e2e and the gates run it from the repository root with --manifest-path, so the
//! search path comes from here instead: DUCKDB_LIB_DIR when set, else Homebrew's prefix.
//!
//! It also names the revision the binaries are built from, MCP_RHAI_REVISION (the `version` of every
//! plan.* event, src/panel_events.rs): git's short HEAD, `-dirty` when this crate had uncommitted
//! changes, `unknown` outside git. It is read again when HEAD moves or a source changes.
use std::path::Path;
use std::process::Command;

fn git(args: &[&str]) -> Option<String> {
    let out = Command::new("git").args(args).output().ok().filter(|o| o.status.success())?;
    Some(String::from_utf8_lossy(&out.stdout).trim().to_string())
}

fn revision() {
    let rev = git(&["rev-parse", "--short=10", "HEAD"]).unwrap_or_else(|| "unknown".into());
    let dirty = git(&["status", "--porcelain", "--", "."]).is_some_and(|s| !s.is_empty());
    println!("cargo:rustc-env=MCP_RHAI_REVISION={rev}{}", if dirty { "-dirty" } else { "" });
    for (flag, files) in [("--git-dir", &["HEAD"][..]), ("--git-common-dir", &["packed-refs", "refs/heads"][..])] {
        if let Some(dir) = git(&["rev-parse", flag]) {
            for f in files {
                let p = Path::new(&dir).join(f);
                if p.exists() { println!("cargo:rerun-if-changed={}", p.display()); }
            }
        }
    }
    println!("cargo:rerun-if-changed=src");
    println!("cargo:rerun-if-changed=Cargo.toml");
    println!("cargo:rerun-if-changed=build.rs");
}

fn main() {
    revision();
    println!("cargo:rerun-if-env-changed=DUCKDB_LIB_DIR");
    println!("cargo:rerun-if-env-changed=HOMEBREW_PREFIX");
    if std::env::var_os("CARGO_FEATURE_STORE").is_none() {
        return;
    }
    let dirs = [
        std::env::var("DUCKDB_LIB_DIR").ok(),
        std::env::var("HOMEBREW_PREFIX").ok().map(|p| format!("{p}/lib")),
        Some("/opt/homebrew/lib".into()),
        Some("/usr/local/lib".into()),
    ];
    match dirs.into_iter().flatten().find(|d| ["libduckdb.dylib", "libduckdb.so"].iter().any(|f| Path::new(d).join(f).is_file())) {
        Some(dir) => println!("cargo:rustc-link-search=native={dir}"),
        None => println!("cargo:warning=no libduckdb found (set DUCKDB_LIB_DIR, `brew install duckdb`, or build with --no-default-features)"),
    }
}
