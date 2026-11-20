#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CACHE="$HOME/.cache/locvas-spike/rich-text-render-spike/rich-text-render-spike"
RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/rich-text-render-bench.XXXXXX")"
CONFIG="$RUN_DIR/config"
trap 'rm -rf "$RUN_DIR"' EXIT
rm -rf "$CACHE"
mkdir -p out

echo "cold run:"
XDG_CONFIG_HOME="$CONFIG" RICH_TEXT_RENDER_DEBUG=1 QT_QPA_PLATFORM=offscreen \
  ./build/rich-text-render --file tests/sources/bench.md --screenshot out/bench-cold.png \
  2>&1 | tee out/bench-cold.log | grep -E '^SUMMARY'

echo "warm run:"
XDG_CONFIG_HOME="$CONFIG" RICH_TEXT_RENDER_DEBUG=1 QT_QPA_PLATFORM=offscreen \
  ./build/rich-text-render --file tests/sources/bench.md --screenshot out/bench-warm.png \
  2>&1 | tee out/bench-warm.log | grep -E '^SUMMARY'
