#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# 30-block regression: the renderer service must keep at most two qjs workers
# and the total process-tree PSS under 128MB.
exec "$ROOT/scripts/mem-bench.sh" tests/sources/bench.md bench30
