#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p out

QJS="third_party/quickjs-ng/build/qjs"

"$QJS" --std renderer/dist/mathjax-qjs.mjs tests/sources/formula.tex display > out/test-math.svg
"$QJS" --std renderer/dist/mermaid-qjs.mjs tests/sources/diagram2.mmd > out/test-mermaid.svg

test -s out/test-math.svg
test -s out/test-mermaid.svg

QT_QPA_PLATFORM=offscreen ./build/svgcheck out/test-math.svg out/test-math.png math
QT_QPA_PLATFORM=offscreen ./build/svgcheck out/test-mermaid.svg out/test-mermaid.png mermaid

test -s out/test-math.png
test -s out/test-mermaid.png

ctest --test-dir build --output-on-failure

echo "renderer and module tests passed"
