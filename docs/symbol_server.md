# OpenTTD's Symbol Server

For all official releases, OpenTTD collects the Breakpad Symbols (SYM-files) and Microsoft's Symbols (PDB-files), and publishes them on our own Symbol Server (https://symbols.openttd.org).

These symbol files are needed to analyze `crash.dmp` files as attached to issues by users.
A `crash.dmp` is created on Windows, Linux, and MacOS when a crash happens.
This combined with the `crash.log` should give a pretty good indication what was going on at the moment the game crashed.

## Analyzing a crash.dmp

### MSVC

In MSVC you can add the above URL as Symbol Server (and please enable MSVC's for all other libraries), allowing you to analyze `crash.dmp`.

Now simply open up the `crash.dmp`, and start debugging.

### All other platforms

The best tool to use is `minidump-stackwalk` as published in the Rust's cargo index:

```bash
cargo install minidump-stackwalk
```

For how to install Rust, please see [here](https://doc.rust-lang.org/cargo/getting-started/installation.html).

Now run the tool like:

```bash
minidump-stackwalk <crash.dmp> --symbols-url https://symbols.openttd.org
```

For convenience, the above Symbol Server also check with Mozilla's Symbol Server in case any other library but OpenTTD is requested.
This means files like `libc`, `kernel32.dll`, etc are all available on the above mentioned Symbol Server.
