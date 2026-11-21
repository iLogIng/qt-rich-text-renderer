#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/rich-text-renderer-math.XXXXXX")"
trap 'rm -rf "$RUN_DIR"' EXIT

LOG="$RUN_DIR/math-delimiters.log"
rm -rf "$HOME/.cache/rich-text-renderer/rich-text-renderer"
if ! XDG_CONFIG_HOME="$RUN_DIR/config" RICH_TEXT_RENDER_DEBUG=1 QT_QPA_PLATFORM=offscreen ./build/rich-text-renderer \
  --file tests/sources/math-delimiters.md \
  --screenshot "$RUN_DIR/math-delimiters.png" >"$LOG" 2>&1; then
  cat "$LOG" >&2
  exit 1
fi

summary=$(grep -E '^SUMMARY ' "$LOG" | tail -1 || true)
echo "$summary"
blocks=$(printf '%s\n' "$summary" | sed -n 's/.*blocks=\([0-9][0-9]*\).*/\1/p')
if [[ "$blocks" != "5" ]]; then
  echo "FAIL: expected 5 math/mermaid blocks, got ${blocks:-none}" >&2
  exit 1
fi
echo "PASS: math delimiter tokenizer produced 5 blocks"
