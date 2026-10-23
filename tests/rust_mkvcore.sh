#!/bin/bash
# Standalone Rust checks with the same SSD toolchain as product builds.
set -eu
cd "$(dirname "$0")/.."
export RUSTUP_HOME="${RUSTUP_HOME:-/Volumes/ExternalSSD/rust/rustup}"
export CARGO_HOME="${CARGO_HOME:-/Volumes/ExternalSSD/rust/cargo}"
export PATH="$CARGO_HOME/bin:/opt/homebrew/opt/rustup/bin:$PATH"
cargo clippy --manifest-path rust/mkvcore/Cargo.toml --all-targets --locked -- -D warnings
cargo test --manifest-path rust/mkvcore/Cargo.toml --locked
