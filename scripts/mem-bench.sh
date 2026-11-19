#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

FILE="${1:-samples/bench.md}"
LABEL="${2:-bench}"
WARM="${WARM:-0}"
MAX_TOTAL_MB="${MAX_TOTAL_MB:-128}"
MAX_WORKERS="${MAX_WORKERS:-2}"

CACHE="$HOME/.cache/locvas-spike/rich-text-render-spike/rich-text-render-spike"
if [[ "$WARM" != "1" ]]; then
  rm -rf "$CACHE"
fi
mkdir -p /tmp/opencode

RICH_TEXT_RENDER_DEBUG=1 QT_QPA_PLATFORM=offscreen ./build/rich-text-render \
  --file "$FILE" --screenshot "/tmp/opencode/mem-${LABEL}.png" \
  >"/tmp/opencode/mem-${LABEL}.log" 2>&1 &
app_pid=$!

python3 - "$app_pid" "$MAX_TOTAL_MB" "$MAX_WORKERS" <<'PY'
import os
import sys
import time

app_pid = int(sys.argv[1])
max_total_mb = float(sys.argv[2])
max_workers = int(sys.argv[3])

max_processes = 0
max_qjs_pss_kb = 0
max_app_pss_kb = 0
max_total_pss_kb = 0

def pss_kb(pid):
    try:
        for line in open(f'/proc/{pid}/smaps_rollup'):
            if line.startswith('Pss:'):
                return int(line.split()[1])
    except OSError:
        pass
    return 0

while True:
    try:
        os.kill(app_pid, 0)
    except OSError:
        break

    qjs_pss = 0
    qjs_count = 0
    for entry in os.listdir('/proc'):
        if not entry.isdigit():
            continue
        try:
            argv = open(f'/proc/{entry}/cmdline', 'rb').read().split(b'\0')
        except OSError:
            continue
        if not argv or not argv[0]:
            continue
        # Count only the actual qjs executable, not a shell command line that
        # happens to contain the string "qjs".
        if os.path.basename(argv[0].decode(errors='ignore')) != 'qjs':
            continue
        qjs_count += 1
        qjs_pss += pss_kb(entry)

    app_pss = pss_kb(app_pid)
    total = qjs_pss + app_pss
    max_processes = max(max_processes, qjs_count)
    max_qjs_pss_kb = max(max_qjs_pss_kb, qjs_pss)
    max_app_pss_kb = max(max_app_pss_kb, app_pss)
    max_total_pss_kb = max(max_total_pss_kb, total)
    time.sleep(0.01)

print(f'max_qjs_processes={max_processes}')
print(f'max_qjs_pss_mb={max_qjs_pss_kb / 1024:.1f}')
print(f'max_app_pss_mb={max_app_pss_kb / 1024:.1f}')
print(f'max_total_pss_mb={max_total_pss_kb / 1024:.1f}')

failed = False
if max_processes > max_workers:
    print(f'FAIL: qjs processes {max_processes} > {max_workers}', file=sys.stderr)
    failed = True
if max_total_pss_kb / 1024 > max_total_mb:
    print(f'FAIL: total PSS {max_total_pss_kb / 1024:.1f}MB > {max_total_mb}MB', file=sys.stderr)
    failed = True
if failed:
    sys.exit(1)
print('PASS: worker count and total PSS within bounds')
PY

wait "$app_pid" 2>/dev/null || true
grep -E '^SUMMARY' "/tmp/opencode/mem-${LABEL}.log" || true
