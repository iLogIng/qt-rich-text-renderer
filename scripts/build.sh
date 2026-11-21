#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Build Quickjs Engine
if [[ ! -x third_party/quickjs-ng/build/qjs ]]; then
  echo "qjs not found; run scripts/setup-quickjs.sh first" >&2
  exit 1
fi

# Find Mathjax and Mermaid qjs surpport
if [[ ! -f renderer/dist/mathjax-qjs.mjs || ! -f renderer/dist/mermaid-qjs.mjs ]]; then
  echo "renderer bundles not found; run scripts/setup-renderer.sh first" >&2
  exit 1
fi

BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"

cmake -S . -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build -j"$(nproc)"
