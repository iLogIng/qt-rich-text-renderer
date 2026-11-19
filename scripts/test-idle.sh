#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CACHE="$HOME/.cache/locvas-spike/rich-text-render-spike/rich-text-render-spike"
rm -rf "$CACHE"
mkdir -p /tmp/opencode

RICH_TEXT_RENDER_IDLE_MS=2000 QT_QPA_PLATFORM=offscreen ./build/rich-text-render \
  --file samples/bench.md --screenshot /tmp/opencode/idle.png \
  >/tmp/opencode/idle.log 2>&1 &
app_pid=$!

sleep 5
count=$(python3 - <<'PY'
import os
n = 0
for entry in os.listdir('/proc'):
    if not entry.isdigit():
        continue
    try:
        argv = open(f'/proc/{entry}/cmdline', 'rb').read().split(b'\0')
    except OSError:
        continue
    if argv and argv[0] and os.path.basename(argv[0].decode(errors='ignore')) == 'qjs':
        n += 1
print(n)
PY
)

wait "$app_pid" 2>/dev/null || true

if [[ "$count" -ne 0 ]]; then
  echo "FAIL: qjs workers still running after idle timeout: $count" >&2
  exit 1
fi
echo "PASS: qjs workers exited after idle timeout"
