#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p /tmp/opencode

LOG=/tmp/opencode/math-delimiters.log
rm -rf "$HOME/.cache/locvas-spike/rich-text-render-spike/rich-text-render-spike"
RICH_TEXT_RENDER_DEBUG=1 QT_QPA_PLATFORM=offscreen ./build/rich-text-render \
  --file tests/sources/math-delimiters.md \
  --screenshot /tmp/opencode/math-delimiters.png >"$LOG" 2>&1

summary=$(grep -E '^SUMMARY ' "$LOG" | tail -1 || true)
echo "$summary"
blocks=$(printf '%s\n' "$summary" | sed -n 's/.*blocks=\([0-9][0-9]*\).*/\1/p')
if [[ "$blocks" != "5" ]]; then
  echo "FAIL: expected 5 math/mermaid blocks, got ${blocks:-none}" >&2
  exit 1
fi
echo "PASS: math delimiter tokenizer produced 5 blocks"
