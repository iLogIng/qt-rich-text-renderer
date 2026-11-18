#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CACHE="$HOME/.cache/locvas-spike/rich-text-render-spike/rich-text-render-spike"
rm -rf "$CACHE"
mkdir -p /tmp/opencode

QT_QPA_PLATFORM=offscreen ./build/rich-text-render \
  --file samples/bench.md \
  --screenshot /tmp/opencode/process-bound.png \
  >/tmp/opencode/process-bound.log 2>&1 &
app_pid=$!

python3 - "$app_pid" <<'PY'
import os
import sys
import time

app_pid = int(sys.argv[1])
max_processes = 0
max_pss_kb = 0

while True:
    try:
        os.kill(app_pid, 0)
    except OSError:
        break

    count = 0
    pss = 0
    for entry in os.listdir('/proc'):
        if not entry.isdigit():
            continue
        try:
            cmdline = open(f'/proc/{entry}/cmdline', 'rb').read().replace(b'\0', b' ').decode(errors='ignore')
        except OSError:
            continue
        if 'quickjs-ng/build/qjs' not in cmdline:
            continue
        count += 1
        try:
            for line in open(f'/proc/{entry}/smaps_rollup'):
                if line.startswith('Pss:'):
                    pss += int(line.split()[1])
                    break
        except OSError:
            pass

    max_processes = max(max_processes, count)
    max_pss_kb = max(max_pss_kb, pss)
    time.sleep(0.02)

print(f'max_qjs_processes={max_processes}')
print(f'max_qjs_pss_mb={max_pss_kb / 1024:.1f}')

# 30 blocks + a startup race allowance. Before the cancellation fix this
# was 34-35 because the sample render was not killed when bench loaded.
if max_processes > 32:
    print('FAIL: stale render processes were not cancelled', file=sys.stderr)
    sys.exit(1)
print('PASS: stale render processes cancelled within bound')
PY

wait "$app_pid" 2>/dev/null || true
cat /tmp/opencode/process-bound.log
