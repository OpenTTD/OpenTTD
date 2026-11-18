//! RESEARCH-ONLY. Where the linker finds the system libduckdb for the `store` feature (ADR 0033).
//!
//! The duckdb crate is used without its bundled C++ build, and libduckdb-sys then asks only for
//! `-lduckdb`. A `.cargo/config.toml` setting DUCKDB_LIB_DIR is read only when cargo runs inside this
//! directory, and mcp-e2e and the gates run it from the repository root with --manifest-path, so the
//! search path comes from here instead: DUCKDB_LIB_DIR when set, else Homebrew's prefix.
use std::path::Path;

fn main() {
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
