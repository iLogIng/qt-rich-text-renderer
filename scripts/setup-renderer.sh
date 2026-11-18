#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT/renderer"

npm install --no-audit --no-fund
npm run build

ls -lh dist/mathjax-qjs.mjs dist/mermaid-qjs.mjs
