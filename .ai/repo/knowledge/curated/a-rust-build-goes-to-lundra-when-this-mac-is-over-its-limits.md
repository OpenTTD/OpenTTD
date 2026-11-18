---
schema: knowledge/v1
id: a-rust-build-goes-to-lundra-when-this-mac-is-over-its-limits
kind: knowledge
class: convention
title: "When this Mac is over its build limits, mcp-rhai is built and tested on lundra, with the owner's libduckdb in user space"
description: "Before a cargo build or test of tools/research/mcp-rhai while disk is under 35 GiB free or load is far above the core count; how the build is offloaded and what it can and cannot prove."
status: verified
epistemics: observed
date: 2026-11-18
tags: [build, offload, lundra, duckdb, rust]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-rhai/build.rs
    - file:tools/research/mcp-rhai/Cargo.toml
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/a-peer-commit-rides-every-push-of-a-shared-checkout.md
---

# Convention

On 2026-11-18 this Mac had 31 GiB free (the machine's limit for heavy work is 35) and a load average of
about 57 on 18 cores, while I0084 needed a compile and tests of `tools/research/mcp-rhai`. The tailnet
host lundra (`korczis@100.65.22.118`, Linux x86_64, 8 cores, glibc 2.39, rustc 1.98.1) was idle but had
no libduckdb, which the `store` feature links (`build.rs` reads `DUCKDB_LIB_DIR`).

The owner was asked (AskUserQuestion, session ff4a0d12-f79a-47bb-932b-ffd3aa94e38a, 2026-11-18T05:1xZ):

> I0084 (hub events to a ZeroMQ bus and a DuckDB store) is Rust work that needs a compile and tests.
> This Mac is over its limits (31 GiB disk free vs 35, load ~57 on 18 cores). The idle tailnet host
> lundra (Linux, 8 cores, load 0.3, 49 GB free, has cargo) lacks the system libduckdb 1.5 the store
> feature links. How should I build it?

Answer: **"Offload to lundra (Recommended)"**, whose option text was: "Download the official libduckdb 1.5
Linux release zip from github.com/duckdb/duckdb into ~/opt/libduckdb on lundra (user space, no sudo), and
build and test the branch there via a git bundle over ssh; nothing pushed."

Done that way: `~/opt/libduckdb-1.5.5` on lundra holds the v1.5.5 `libduckdb-linux-amd64.zip`
(sha256 `1fb8ce388157d84a25abe685a8a2520bf00c00321821968e4bb398fd766e7abb`; the Mac's Homebrew duckdb
is 1.5.5 too), `~/build/OpenTTD` is a blobless clone of the public fork, and `~/build/target-mcp-rhai`
is its one target directory.

# How to apply it

- Send the worktree, never push: its commits past `origin/master` as a git bundle, its uncommitted
  changes as a binary diff plus a tar of untracked files; on lundra check out the head, apply both, and
  run cargo with `DUCKDB_LIB_DIR` and `LD_LIBRARY_PATH` set to `~/opt/libduckdb-1.5.5`.
- A pass there proves the Rust compiles and its unit tests pass on Linux. It does not prove a check
  that needs the macOS OpenTTD binary or the live panel (`tools/research/mcp-hub-check` and the like):
  run those here, or say they were not run.
- Report the host with every result, as `lundra: cargo test ... at <sha>`.
