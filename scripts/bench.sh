#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CACHE="$HOME/.cache/locvas-spike/rich-text-render-spike/rich-text-render-spike"
rm -rf "$CACHE"
mkdir -p out

echo "cold run:"
RICH_TEXT_RENDER_DEBUG=1 QT_QPA_PLATFORM=offscreen \
  ./build/rich-text-render --file samples/bench.md --screenshot out/bench-cold.png \
  2>&1 | tee out/bench-cold.log | grep -E '^SUMMARY'

echo "warm run:"
RICH_TEXT_RENDER_DEBUG=1 QT_QPA_PLATFORM=offscreen \
  ./build/rich-text-render --file samples/bench.md --screenshot out/bench-warm.png \
  2>&1 | tee out/bench-warm.log | grep -E '^SUMMARY'
