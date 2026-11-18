#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -x third_party/quickjs-ng/build/qjs ]]; then
  echo "qjs not found; run scripts/setup-quickjs.sh first" >&2
  exit 1
fi

if [[ ! -f renderer/dist/mathjax-qjs.mjs || ! -f renderer/dist/mermaid-qjs.mjs ]]; then
  echo "renderer bundles not found; run scripts/setup-renderer.sh first" >&2
  exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
